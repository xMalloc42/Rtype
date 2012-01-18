#include	"Server.hpp"

Server	*Server::_singleton = NULL;

Server	*Server::getInstance()
{
	if (_singleton == NULL)
		_singleton = new Server();
	return _singleton;
}

void	Server::kill()
{
	if (_singleton != NULL)
	{
		delete	_singleton;
		_singleton = NULL;
	}
}

Server::Server(void)
{
#ifdef __linux__
	threadPoolMutex = new UnixMutex();
	socketMutex = new UnixMutex();
	sharedMonsterFirstTypeMutex = new UnixMutex();
	sharedMonsterSecondTypeMutex = new UnixMutex();
	playersMutex = new UnixMutex();
	gamesMutex = new UnixMutex();
#else
	threadPoolMutex = new WinMutex();
	socketMutex = new WinMutex();
	sharedMonsterFirstTypeMutex = new WinMutex();
	sharedMonsterSecondTypeMutex = new WinMutex();
	playersMutex = new WinMutex();
	gamesMutex = new WinMutex();
#endif
	threadPoolMutex->init();
	socketMutex->init();
	sharedMonsterFirstTypeMutex->init();
	sharedMonsterSecondTypeMutex->init();
	playersMutex->init();
	gamesMutex->init();

	AutoMutex	am1(threadPoolMutex);
	threadPool = ThreadPool_::getInstance();

	AutoMutex	am3(sharedMonsterFirstTypeMutex);
	AutoMutex	am4(sharedMonsterSecondTypeMutex);
#ifdef __linux__
	sharedLibMonsterFirstType = new LinuxDynLib();
	sharedLibMonsterSecondType = new LinuxDynLib();
#else
	sharedLibMonsterFirstType = new WindowsDynLib();
	sharedLibMonsterSecondType = new WindowsDynLib();
#endif

	AutoMutex	am2(socketMutex);
#ifdef	__linux__
	socket = new UnixSocket();
#else
	socket = new WinSocket();
#endif
}

Server::~Server(void)
{
	playersMutex->lock();
	for (std::vector<Player *>::const_iterator it = players.begin(); it != players.end(); ++it)
		delete *it;
	playersMutex->unlock();
	gamesMutex->lock();
	for (std::vector<Game *>::const_iterator it = games.begin(); it != games.end(); ++it)
		delete *it;
	gamesMutex->unlock();
	socketMutex->lock();
	delete socket;
	socketMutex->unlock();
	sharedMonsterFirstTypeMutex->lock();
	sharedLibMonsterFirstType->closeLib();
	delete sharedLibMonsterFirstType;
	sharedMonsterFirstTypeMutex->unlock();
	sharedMonsterSecondTypeMutex->lock();
	sharedLibMonsterSecondType->closeLib();
	delete sharedLibMonsterSecondType;
	sharedMonsterSecondTypeMutex->unlock();
	threadPoolMutex->lock();
	ThreadPool_::kill();
	threadPoolMutex->unlock();

	threadPoolMutex->destroy();
	socketMutex->destroy();
	sharedMonsterFirstTypeMutex->destroy();
	sharedMonsterSecondTypeMutex->destroy();
	playersMutex->destroy();
	gamesMutex->destroy();

	delete threadPoolMutex;
	delete socketMutex;
	delete sharedMonsterFirstTypeMutex;
	delete sharedMonsterSecondTypeMutex;
	delete playersMutex;
	delete gamesMutex;
}

void	Server::init(void)
{	
	threadPoolMutex->lock();
	threadPool->ThreadPoolInit(10);
	threadPoolMutex->unlock();
	std::string	libName1 = "DLL/CreateMonsterFirstTypeDLL";
	std::string libName2 = "DLL/CreateMonsterSecondTypeDLL";
#ifdef __linux__
	libName1 += ".so";
	libName2 += ".so";
#else
	libName1 += ".dll";
	libName2 += ".dll";
#endif

	AutoMutex	am2(sharedMonsterFirstTypeMutex);
	if (!(sharedLibMonsterFirstType->openLib(libName1)))
		throw std::exception();
//	AutoMutex	am3(sharedMonsterSecondTypeMutex);
//	if (!(sharedLibMonsterSecondType->openLib(libName2)))
//		throw std::exception();
	sharedLibMonsterFirstType->setSymbolName("getInstanceDLL");
//	sharedLibMonsterSecondType->setSymbolName("getInstanceDLL");

	AutoMutex	am(socketMutex);
	if (!(socket->CreateSocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)))
		throw std::exception();
	if (!(socket->BindSocket(AF_INET, 4242)))
		throw std::exception();
}

void	Server::running(void)
{
  RTProtocol::Header		*h;
  Command		*c = Command::getInstance();
  
	threadPoolMutex->lock();
	threadPool->QueuePush(Server::checkConnectionClients, NULL);
	threadPool->QueuePush(Server::sendUpdateClients, NULL);
	threadPoolMutex->unlock();
  while (1)
    {
//		socketMutex->lock();
		receive rHeader = socket->RecvData(sizeof(RTProtocol::Header), MSG_PEEK);
//		socketMutex->unlock();
		h = reinterpret_cast<RTProtocol::Header *>(rHeader.data_);
//		socketMutex->lock();
		//if (receive.data_ != NULL)
		receive rBody = socket->RecvData(h->Size, 0);
//		socketMutex->unlock();
		if (rBody.data_ != NULL)
		{
			_command	*recv = new _command();
			recv->h = h;
			recv->r = rBody;
//			threadPoolMutex->lock();
			threadPool->QueuePush(Command::FindCommand, recv);
//			threadPoolMutex->unlock();
		}
    }
}

#ifdef __linux__
#include <unistd.h>
#endif

void	Server::checkConnectionClients(void *param)
{
  Command *c = Command::getInstance();
  Server	*s = Server::getInstance();

	while (1)
    {
		for (unsigned int i = 0; i < s->getPlayers().size(); ++i)
		{
			if (s->getPlayer(i)->getConnect() == true)
			{
				s->getPlayer(i)->setConnect(false);
//				std::cout << "CONNECT" << std::endl;
			}
			else if (s->getPlayer(i)->getIdGame() != -1)
			{
//				std::cout << "DISCONNECT" << std::endl;
				for (int j = 0; j < 4; ++j)
					if (s->getGame(s->getPlayer(i))->getPlayer(i) != 0)
					{
//						std::cout << "LOGOUT: " << i << std::endl;
						c->SendConnection(s->getPlayer(i), RTProtocol::LOG_OUT);
					}
				s->getGame(s->getPlayer(i))->setPlayer(0, s->getPlayer(i)->getId());
				s->deletePlayer(s->getPlayer(i));
			}
			else
				s->deletePlayer(s->getPlayer(i));
		}
#ifdef __linux__
      sleep(12);
#else
      Sleep(12000);
#endif
    }
}

void	Server::sendUpdateClients(void *param)
{
  struct timeval	now;
  struct timeval	diff;
  
  Command	*c = Command::getInstance();
  Server	*s = Server::getInstance();
  
  while (1)
    {
		gettimeofday(&now, NULL);
		for (unsigned int i = 0; i < s->getGames().size(); ++i)
			if (s->getGame(i) != NULL)
				s->getGame(i)->update();
		for (unsigned int i = 0; i < s->getPlayers().size(); ++i)
			if (s->getPlayer(i)->getIdGame() != -1)
				c->SendGameData(s->getPlayer(i));
		gettimeofday(&diff, NULL);
		diff.tv_sec = diff.tv_sec - now.tv_sec;
		if (diff.tv_sec > 0)
		{
			diff.tv_usec += diff.tv_sec * 1000000;
			diff.tv_sec = 0;
		}
		diff.tv_usec = (1000000 - (diff.tv_usec - now.tv_usec) ) / 120;
#ifdef __linux__
		usleep(diff.tv_usec);
#else
		Sleep(diff.tv_usec / 1000);
#endif
    }
}

IDynLib	*Server::getSharedLibMonsterFirstType(void) const
{
	return sharedLibMonsterFirstType;
}

IDynLib	*Server::getSharedLibMonsterSecondType(void) const
{
	return sharedLibMonsterSecondType;
}

void	Server::addNewPlayer(const struct sockaddr_in rcv)
{
	AutoMutex	am(playersMutex);
	if (getPlayer(rcv) == 0)
		players.push_back(new Player(rcv));
}

Player	*Server::getPlayer(const struct sockaddr_in rcv) const
{
	playersMutex->lock();
	for (std::vector<Player *>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
		playersMutex->unlock();
		Player *p = *it;
		uint32_t u = p->getSockaddr().sin_addr.s_addr;
		if (p->getSockaddr().sin_addr.s_addr == rcv.sin_addr.s_addr && p->getSockaddr().sin_port == rcv.sin_port)
			return p;
		playersMutex->lock();
    }
	playersMutex->unlock();
	return 0;
}

Player	*Server::getPlayer(const unsigned int it) const
{
	AutoMutex	am(playersMutex);
	if (players.size() > it)
		return players[it];
	return NULL;
}

std::vector<Player *>	Server::getPlayers(void) const
{
	AutoMutex	am(playersMutex);
	return players;
}

void	Server::deletePlayer(const Player *p)
{
	AutoMutex	am(playersMutex);
	for (std::vector<Player *>::iterator it = players.begin(); it != players.end(); ++it)
	{
		playersMutex->unlock();
		if (p == *it)
		{
			playersMutex->lock();
			players.erase(it);
			break;
		}
		playersMutex->lock();
	}
}

void	Server::addNewGame(Player *p, int id)
{
	AutoMutex	am(gamesMutex);
	games.push_back(new Game(p, id));
	p->setIdGame(id);
	p->setId(RTProtocol::PLAYER_1);
}

Game	*Server::getGame(unsigned int i) const
{
	AutoMutex	am(gamesMutex);
	if (i < games.size())
		return games[i];
	return NULL;
}

Game	*Server::getGame(const Player *p) const
{
	AutoMutex	am(gamesMutex);
	for (std::vector<Game *>::const_iterator it = games.begin(); it != games.end(); ++it)
	{
		if ((*it)->getId() == p->getIdGame())
			return ((*it));
	}
	return 0;
}

std::vector<Game *>		Server::getGames(void) const
{
	AutoMutex	am(gamesMutex);
	return games;
}

void	Server::deleteGame(const Game *g)
{
	AutoMutex	am(gamesMutex);
	for (std::vector<Game *>::iterator it = games.begin(); it != games.end(); ++it)
	{
		gamesMutex->unlock();
		if (g == *it)
		{
			gamesMutex->lock();
			games.erase(it);
			break;
		}
		gamesMutex->lock();
	}
}

int		Server::getAvailableId(void) const
{
	int	id = 0;
  	AutoMutex	am(gamesMutex);
	for (std::vector<Game *>::const_iterator it = games.begin(); it != games.end(); ++it)
		if ((*it)->getId() >= id)
			++id;
	return id;
}

Game	*Server::getAvailableSlot(void) const
{
	AutoMutex	am(gamesMutex);
	for (std::vector<Game *>::const_iterator it = games.begin(); it != games.end(); ++it)
		if ((*it)->getEmptySlot() != -1)
		return (*it);
	return 0;
}

ISocket	*Server::getSocket(void) const
{
	AutoMutex	am(socketMutex);
	return socket;
}

#ifndef	__linux__

int		gettimeofday (struct timeval *tp, void *tz)
{
  struct _timeb timebuffer;
  _ftime_s(&timebuffer);
  tp->tv_sec = timebuffer.time;
  tp->tv_usec = timebuffer.millitm * 1000;
  return 0;
}

#endif
