#include <list>
#include <winsock2.h>
#include <process.h>
#include <conio.h>
#include <time.h>
using namespace std;
#pragma warning(disable : 4996)
#pragma comment(lib, "ws2_32")
//--------------------------------
// �\���̃^�O�錾
//--------------------------------
// �N���C�A���g�̏����Z�b�g���邽�߂̂̃N���X
struct ClientInfo {
	int guu_qty;	// �O�[���o������
	int tyoki_qty;	// �`���L���o������
	int paa_qty;	// �p�[���o������
	SOCKET clientsocket; // �N���C�A���g�Ƃ���肷�邽�߂̃\�P�b�g�ԍ�
	SOCKADDR_IN clientaddr; // �N���C�A���g���̃A�h���X���
	int clientaddrlength; // �N���C�A���g�̃A�h���X��
	char recvbuffer[1024]; // ��M�o�b�t�@
	int zyanken; // 0:�O�[�@1:�`���L�@2:�p�[
	int kati; // ������
	int make; // ������
	int wake; // ��������
	int toukeiindex; // ���v�C���f�b�N�X
					 // �R���X�g���N�^
	ClientInfo() {
		// ��M�A�h�������Z�b�g
		clientaddrlength = sizeof(SOCKADDR_IN);
	}
};
//--------------------------------
// �v���g�^�C�v�錾
//--------------------------------
void tcpsnd(SOCKET s, char* sendbuf);
int tcprcv(SOCKET s, char* buf);
void clientsub(void* p1);
void acceptsub(void* p1);
void getip(void);
void dispclientinfo(ClientInfo* clientinfo);
//--------------------------------
// �O���[�o���ϐ�
//--------------------------------
SOCKET g_mysock; // �T�[�o�̃\�P�b�g
const u_short g_myportno = 20045; // �T�[�o�̃|�[�g�ԍ�
SOCKADDR_IN g_myaddr; // �T�[�o�̃A�h���X
list<ClientInfo*> g_clientlist; // �N���C�A���g���̃��X�g�\��
list<ClientInfo*>::iterator g_ite; // �N���C�A���g���̃C�e���[�^
CRITICAL_SECTION g_sect; // �N���e�B�J���Z�N�V����
						 ////////////////////////////////////////
						 // �����̂h�o�A�h���X���擾�@�@�@�@ //
						 ////////////////////////////////////////
void getip()
{
	char szBuf[128];
	char szIP[128];
	LPHOSTENT lpHost;
	IN_ADDR inaddr;
	// ���[�J���z�X�g�����擾����
	gethostname(szBuf, sizeof(szBuf));
	printf("\n�z�X�g��=%s\n", szBuf);
	// �h�o�A�h���X�擾
	lpHost = gethostbyname(szBuf);
	memcpy(&inaddr, lpHost->h_addr_list[0], 4);
	strcpy(szIP, inet_ntoa(inaddr));
	printf("IP�A�h���X=%s\n", szIP);
	printf("�|�[�g�ԍ�=%d \n\n", g_myportno);
}
////////////////////////////////////////
// �f�[�^���M //
// p1 : �\�P�b�g //
// p2 : ���M�o�b�t�@ //
////////////////////////////////////////
void tcpsnd(SOCKET s, char* sendbuf)
{
	char* buf; // ���M�o�b�t�@�擪�A�h���X
	int sendlen; // ���M�����f�[�^��
	int len = strlen(sendbuf); // ���M�o�b�t�@��
	buf = sendbuf;
	do {
		sendlen = send(s, buf, len, 0);
		len -= sendlen;
		buf += sendlen;
	} while (len>0);
}
//////////////////////////////////////////////////
// �f�[�^��M�i�b�q�k�e�܂�) //
// �s�b�o�X�g���[���f�[�^����M���� //
// ���� //
// p1 : �\�P�b�g //
// p2 : ��M�o�b�t�@ //
// �߂�l //
// �@��M�f�[�^�� //
// (�O�ȉ� �F �G���[����) //
/////////////////////////////////////////////////
int tcprcv(SOCKET s, char* buf)
{
	char* bufstart = buf; // ��M�o�b�t�@�̐擪�A�h���X��ۑ�
	int rcv = 0; // ��M�o�C�g��
	while (1) {
		rcv = recv(s, buf, 1, 0); // �P�o�C�g�Â�M
		if (rcv <= 0) goto end; // ��M�G���[����
		if (*buf == '\r') { // ��؂蕶�����f�i0x0d�j
			rcv = recv(s, buf, 1, 0);
			if (rcv <= 0) goto end; // ��M�G���[����
			if (*buf == '\n') { // ��؂蕶�����f (0x0a)
				break;
			}
			buf += rcv;
		}
		buf += rcv; // ��M�擪�o�C�g��i�߂�
	}
	*buf = 0; // ������̍Ō��NULL���Z�b�g����
	printf("%s \n", bufstart);
end:
	return rcv;
}
//////////////////////////////////////////////////////////
// �N���C�A���g�̃A�h���X����\�����鏈�� //
// ���� //
// p1 : �ڑ��v�����󂯕t�������̃N���C�A���g��� //
// �߂�l //
// �Ȃ� //
//////////////////////////////////////////////////////////
void dispclientinfo(ClientInfo* clientinfo)
{
	// �N���C�A���g�̂h�o�A�h���X��\��
	printf(" �N���C�A���g�h�o %s \n", inet_ntoa(clientinfo->clientaddr.sin_addr));
	// �N���C�A���g�̃|�[�g����\��
	printf(" �N���C�A���g�|�[�g�ԍ� %d \n", ntohs(clientinfo->clientaddr.sin_port));
}
//////////////////////////////////////////////////////////
// ���v���ŉ����o�������߂� //
// ���� //
// p1 : �ڑ��v�����󂯕t�������̃N���C�A���g��� //
// �߂�l //
// �Ȃ� //
//////////////////////////////////////////////////////////
int toukeizyanken(ClientInfo* obj)
{
	const float total = static_cast<float>(obj->guu_qty + obj->tyoki_qty + obj->paa_qty);
	if (total <= 0)
	{
		return rand() % 3;
	}

	const float guu_per = obj->guu_qty / total;
	const float tyoki_per = obj->tyoki_qty / total;
	const float paa_per = obj->paa_qty / total;
	const float random = (rand() % 100000) / 100000.0f;
	if (guu_per >= random)
	{
		return 2;
	}
	if (guu_per + tyoki_per >= random)
	{
		return 0;
	}
	return 1;
}
//////////////////////////////////////////////////////////
// �N���C�A���g�Ƃ������s���ׂ̏��� //
// ���� //
// p1 : �ڑ��v�����󂯕t�������̃N���C�A���g��� //
// �߂�l //
// �Ȃ� //
//////////////////////////////////////////////////////////
void clientsub(void* p1)
{
	list<ClientInfo*>::iterator ite; // ���X�g�\���C�e���[�^
	char buf[64];
	char buf1[64];
	char buf2[64];
	int i = 0;
	int sts;
	char mojiretu[64];
	int clientzyanken;
	ClientInfo* tempobj; // �N���C�A���g���i�[�p
	ClientInfo* obj = (ClientInfo*)p1; // �����œn���Ă����N���C�A���g�����o��
	printf("�N���C�A���g�T�u�@�X���b�h�X�^�[�g\n");
	// �������N���A
	srand((unsigned)time(NULL));
	// ���������������[���N���A
	obj->kati = 0;
	obj->make = 0;
	obj->wake = 0;
	// �o������̏���������
	obj->guu_qty = 0;
	obj->tyoki_qty = 0;
	obj->paa_qty = 0;
	// �N���C�A���g�̏����o�͂���
	dispclientinfo(obj);
	/* ---------------------------------
	0:�O�[
	1:�`���L
	2:�p�[
	---------------------------------*/
	while (1) {
		obj->zyanken = toukeizyanken(obj);
		sts = tcprcv(obj->clientsocket, obj->recvbuffer); // ��M
		if (sts <= 0) break;
		clientzyanken = -1;
		sscanf(obj->recvbuffer, "%d", &clientzyanken); // �W�����P���̒l���擾
		switch (clientzyanken) {
		case 0: // �O�[
			obj->guu_qty += 1;
			// �O�[
			if (obj->zyanken == 0) {
				obj->wake++;
				strcpy(mojiretu, "��������");
			}
			// �`���L
			if (obj->zyanken == 1) {
				obj->make++;
				strcpy(mojiretu, "���Ȃ��̏���");
			}
			// �p�[
			if (obj->zyanken == 2) {
				obj->kati++;
				strcpy(mojiretu, "�R���s���[�^�̏���");
			}
			break;
		case 1: // �`���L
			obj->tyoki_qty += 1;
			// �O�[
			if (obj->zyanken == 0) {
				obj->kati++;
				strcpy(mojiretu, "�R���s���[�^�̏���");
			}
			// �`���L
			if (obj->zyanken == 1) {
				obj->wake++;
				strcpy(mojiretu, "��������");
			}
			// �p�[
			if (obj->zyanken == 2) {
				obj->make++;
				strcpy(mojiretu, "���Ȃ��̏���");
			}
			break;
		case 2: // �p�[
			obj->paa_qty += 1;
			// �O�[
			if (obj->zyanken == 0) {
				obj->make++;
				strcpy(mojiretu, "���Ȃ��̏���");
			}
			// �`���L
			if (obj->zyanken == 1) {
				obj->kati++;
				strcpy(mojiretu, "�R���s���[�^�̏���");
			}
			// �p�[
			if (obj->zyanken == 2) {
				strcpy(mojiretu, "��������");
				obj->wake++;
			}
			break;
		default:
			strcpy(mojiretu, "�ԈႦ�Ȃ��Łi0�F�O�[�C1�F�`���L�C2�F�p�[�j���");
			break;
		}
		switch (obj->zyanken) {
		case 0:
			strcpy(buf1, "�O�[");
			break;
		case 1:
			strcpy(buf1, "�`���L");
			break;
		case 2:
			strcpy(buf1, "�p�[");
			break;
		}
		switch (clientzyanken) {
		case 0:
			strcpy(buf2, "�O�[");
			break;
		case 1:
			strcpy(buf2, "�`���L");
			break;
		case 2:
			strcpy(buf2, "�p�[");
			break;
		}
		sprintf(buf, "�R���s���[�^�� %s ���Ȃ��� %s \r\n", buf1, buf2, clientzyanken);
		tcpsnd(obj->clientsocket, buf); // �������b�Z�[�W���M
		sprintf(buf, "%s�@%d��%d��%d���� \r\n", mojiretu, obj->make, obj->kati, obj->wake);
		tcpsnd(obj->clientsocket, buf); // �������b�Z�[�W���M
		i++;
	}
	EnterCriticalSection(&g_sect); // �r������J�n
								   // �Y���N���C�A���g�������X�g�\������폜
	for (ite = g_clientlist.begin(); ite != g_clientlist.end(); ite++) {
		tempobj = *ite;
		if (tempobj->clientaddr.sin_addr.S_un.S_addr == obj->clientaddr.sin_addr.S_un.S_addr) {
			if (tempobj->clientaddr.sin_port == obj->clientaddr.sin_port) {
				g_clientlist.erase(ite);
				break;
			}
		}
	}
	LeaveCriticalSection(&g_sect); // �r������I��
	shutdown(obj->clientsocket, SD_BOTH);
	closesocket(obj->clientsocket);
	delete obj;
	printf("�N���C�A���g�T�u�@�X���b�h�I��\n");
	_endthread();
}
//////////////////////////////////////////////////////////////////
// �N���C�A���g����̐ڑ��v�����󂯕t���邽�߂̃X���b�h //
// ���� //
// p1 : �m�t�k�k //
// �߂�l //
// �Ȃ� //
//////////////////////////////////////////////////////////////////
void acceptsub(void* p1)
{
	ClientInfo* clientobj; // �N���C�A���g�̏����i�[���邽�߂̃G���A
	printf("�A�N�Z�v�g���� �J�n\n");
	g_ite = g_clientlist.begin(); // ���X�g�\���̐擪���擾
	ClientInfo* tempobj; // �N���C�A���g���i�[�p
	while (1) {
		// �N���C�A���g���𐶐�
		clientobj = new ClientInfo();
		// �N���C�A���g����̐ڑ��v�����󂯕t����
		clientobj->clientsocket = accept(g_mysock, (sockaddr*)&clientobj->clientaddr, &clientobj
			->clientaddrlength);
		// �G���[�����������ꍇ�ɂ́A���[�v�𔲂���
		if (clientobj->clientsocket == INVALID_SOCKET) {
			delete clientobj;
			break;
		}
		// �N���C�A���g����̐ڑ��v�����������邽�߂̃X���b�h���X�^�[�g������
		_beginthread(clientsub, 400, clientobj);
		// �N���C�A���g�������X�g�\���ɃZ�b�g���邽�߁A�r������J�n
		EnterCriticalSection(&g_sect);
		// �N���C�A���g�������X�g�\���ɂȂ�
		g_clientlist.insert(g_ite, clientobj);
		printf("���X�g�� %d \n", g_clientlist.size());
		// �r������I��
		LeaveCriticalSection(&g_sect);
	}
	EnterCriticalSection(&g_sect); // �r������J�n
								   // ���ׂẴN���C�A���g�ɂ��ă\�P�b�g�I���������s��
	for (g_ite = g_clientlist.begin(); g_ite != g_clientlist.end(); g_ite++) {
		tempobj = *g_ite;
		shutdown(tempobj->clientsocket, SD_BOTH);
		closesocket(tempobj->clientsocket);
	}
	LeaveCriticalSection(&g_sect); // �r������I��
	printf("�A�N�Z�v�g���� �I��\n");
	_endthread();
}
/////////////////////////////////
// ���C���֐� //
/////////////////////////////////
void main()
{
	WSADATA wd;
	int y;
	// �N���e�B�J���Z�N�V�����𐶐�����B
	InitializeCriticalSection(&g_sect);
	// WINSOCK��������
	WSAStartup(MAKEWORD(1, 1), &wd);
	// �h�o�A�h���X��\������
	getip();
	// �\�P�b�g����
	g_mysock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	// �\�P�b�g�ɂh�o�A�h���X�A�|�[�g�ԍ����Z�b�g����
	g_myaddr.sin_family = AF_INET;
	g_myaddr.sin_port = htons(g_myportno);
	g_myaddr.sin_addr.s_addr = INADDR_ANY;
	bind(g_mysock, (LPSOCKADDR)&g_myaddr, sizeof(SOCKADDR_IN));
	// �ڑ��v����t�J�n
	listen(g_mysock, SOMAXCONN);
	// �ڑ��v�����󂯂邽�߂̃X���b�h���J�n
	_beginthread(acceptsub, 100, NULL);
	// �I����҂B
	while (1) {
		printf("�I�������鎞��Y�Ɠ��͂��Ă��������B\n");
		y = getch();
		if (y == 'Y') {
			break;
		}
	}
	// �V���b�g�_�E������
	shutdown(g_mysock, SD_BOTH);
	// �\�P�b�g���N���[�Y������
	closesocket(g_mysock);
	// WINSOCK�I������
	WSACleanup();
	// �N���e�B�J���Z�N�V�������폜����
	DeleteCriticalSection(&g_sect);
}