#include <list>
#include <winsock2.h>
#include <process.h>
#include <conio.h>
#include <time.h>
using namespace std;
#pragma warning(disable : 4996)
#pragma comment(lib, "ws2_32")
//--------------------------------
// 構造体タグ宣言
//--------------------------------
// クライアントの情報をセットするためののクラス
struct ClientInfo {
	int guu_qty;	// グーを出した数
	int tyoki_qty;	// チョキを出した数
	int paa_qty;	// パーを出した数
	SOCKET clientsocket; // クライアントとやり取りするためのソケット番号
	SOCKADDR_IN clientaddr; // クライアント側のアドレス情報
	int clientaddrlength; // クライアントのアドレス長
	char recvbuffer[1024]; // 受信バッファ
	int zyanken; // 0:グー　1:チョキ　2:パー
	int kati; // 勝ち数
	int make; // 負け数
	int wake; // 引き分け
	int toukeiindex; // 統計インデックス
					 // コンストラクタ
	ClientInfo() {
		// 受信アドレ長をセット
		clientaddrlength = sizeof(SOCKADDR_IN);
	}
};
//--------------------------------
// プロトタイプ宣言
//--------------------------------
void tcpsnd(SOCKET s, char* sendbuf);
int tcprcv(SOCKET s, char* buf);
void clientsub(void* p1);
void acceptsub(void* p1);
void getip(void);
void dispclientinfo(ClientInfo* clientinfo);
//--------------------------------
// グローバル変数
//--------------------------------
SOCKET g_mysock; // サーバのソケット
const u_short g_myportno = 20045; // サーバのポート番号
SOCKADDR_IN g_myaddr; // サーバのアドレス
list<ClientInfo*> g_clientlist; // クライアント情報のリスト構造
list<ClientInfo*>::iterator g_ite; // クライアント情報のイテレータ
CRITICAL_SECTION g_sect; // クリティカルセクション
						 ////////////////////////////////////////
						 // 自分のＩＰアドレスを取得　　　　 //
						 ////////////////////////////////////////
void getip()
{
	char szBuf[128];
	char szIP[128];
	LPHOSTENT lpHost;
	IN_ADDR inaddr;
	// ローカルホスト名を取得する
	gethostname(szBuf, sizeof(szBuf));
	printf("\nホスト名=%s\n", szBuf);
	// ＩＰアドレス取得
	lpHost = gethostbyname(szBuf);
	memcpy(&inaddr, lpHost->h_addr_list[0], 4);
	strcpy(szIP, inet_ntoa(inaddr));
	printf("IPアドレス=%s\n", szIP);
	printf("ポート番号=%d \n\n", g_myportno);
}
////////////////////////////////////////
// データ送信 //
// p1 : ソケット //
// p2 : 送信バッファ //
////////////////////////////////////////
void tcpsnd(SOCKET s, char* sendbuf)
{
	char* buf; // 送信バッファ先頭アドレス
	int sendlen; // 送信完了データ長
	int len = strlen(sendbuf); // 送信バッファ長
	buf = sendbuf;
	do {
		sendlen = send(s, buf, len, 0);
		len -= sendlen;
		buf += sendlen;
	} while (len>0);
}
//////////////////////////////////////////////////
// データ受信（ＣＲＬＦまで) //
// ＴＣＰストリームデータを受信する //
// 引数 //
// p1 : ソケット //
// p2 : 受信バッファ //
// 戻り値 //
// 　受信データ長 //
// (０以下 ： エラー発生) //
/////////////////////////////////////////////////
int tcprcv(SOCKET s, char* buf)
{
	char* bufstart = buf; // 受信バッファの先頭アドレスを保存
	int rcv = 0; // 受信バイト数
	while (1) {
		rcv = recv(s, buf, 1, 0); // １バイトづつ受信
		if (rcv <= 0) goto end; // 受信エラー発生
		if (*buf == '\r') { // 区切り文字判断（0x0d）
			rcv = recv(s, buf, 1, 0);
			if (rcv <= 0) goto end; // 受信エラー発生
			if (*buf == '\n') { // 区切り文字判断 (0x0a)
				break;
			}
			buf += rcv;
		}
		buf += rcv; // 受信先頭バイトを進める
	}
	*buf = 0; // 文字列の最後にNULLをセットする
	printf("%s \n", bufstart);
end:
	return rcv;
}
//////////////////////////////////////////////////////////
// クライアントのアドレス情報を表示する処理 //
// 引数 //
// p1 : 接続要求を受け付けた時のクライアント情報 //
// 戻り値 //
// なし //
//////////////////////////////////////////////////////////
void dispclientinfo(ClientInfo* clientinfo)
{
	// クライアントのＩＰアドレスを表示
	printf(" クライアントＩＰ %s \n", inet_ntoa(clientinfo->clientaddr.sin_addr));
	// クライアントのポート情報を表示
	printf(" クライアントポート番号 %d \n", ntohs(clientinfo->clientaddr.sin_port));
}
//////////////////////////////////////////////////////////
// 統計情報で何を出すか決める //
// 引数 //
// p1 : 接続要求を受け付けた時のクライアント情報 //
// 戻り値 //
// なし //
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
// クライアントとやり取りを行う為の処理 //
// 引数 //
// p1 : 接続要求を受け付けた時のクライアント情報 //
// 戻り値 //
// なし //
//////////////////////////////////////////////////////////
void clientsub(void* p1)
{
	list<ClientInfo*>::iterator ite; // リスト構造イテレータ
	char buf[64];
	char buf1[64];
	char buf2[64];
	int i = 0;
	int sts;
	char mojiretu[64];
	int clientzyanken;
	ClientInfo* tempobj; // クライアント情報格納用
	ClientInfo* obj = (ClientInfo*)p1; // 引数で渡ってきたクライアント情報取り出し
	printf("クライアントサブ　スレッドスタート\n");
	// 乱数をクリア
	srand((unsigned)time(NULL));
	// 勝ち数負け数をゼロクリア
	obj->kati = 0;
	obj->make = 0;
	obj->wake = 0;
	// 出した手の情報を初期化
	obj->guu_qty = 0;
	obj->tyoki_qty = 0;
	obj->paa_qty = 0;
	// クライアントの情報を出力する
	dispclientinfo(obj);
	/* ---------------------------------
	0:グー
	1:チョキ
	2:パー
	---------------------------------*/
	while (1) {
		obj->zyanken = toukeizyanken(obj);
		sts = tcprcv(obj->clientsocket, obj->recvbuffer); // 受信
		if (sts <= 0) break;
		clientzyanken = -1;
		sscanf(obj->recvbuffer, "%d", &clientzyanken); // ジャンケンの値を取得
		switch (clientzyanken) {
		case 0: // グー
			obj->guu_qty += 1;
			// グー
			if (obj->zyanken == 0) {
				obj->wake++;
				strcpy(mojiretu, "引き分け");
			}
			// チョキ
			if (obj->zyanken == 1) {
				obj->make++;
				strcpy(mojiretu, "あなたの勝ち");
			}
			// パー
			if (obj->zyanken == 2) {
				obj->kati++;
				strcpy(mojiretu, "コンピュータの勝ち");
			}
			break;
		case 1: // チョキ
			obj->tyoki_qty += 1;
			// グー
			if (obj->zyanken == 0) {
				obj->kati++;
				strcpy(mojiretu, "コンピュータの勝ち");
			}
			// チョキ
			if (obj->zyanken == 1) {
				obj->wake++;
				strcpy(mojiretu, "引き分け");
			}
			// パー
			if (obj->zyanken == 2) {
				obj->make++;
				strcpy(mojiretu, "あなたの勝ち");
			}
			break;
		case 2: // パー
			obj->paa_qty += 1;
			// グー
			if (obj->zyanken == 0) {
				obj->make++;
				strcpy(mojiretu, "あなたの勝ち");
			}
			// チョキ
			if (obj->zyanken == 1) {
				obj->kati++;
				strcpy(mojiretu, "コンピュータの勝ち");
			}
			// パー
			if (obj->zyanken == 2) {
				strcpy(mojiretu, "引き分け");
				obj->wake++;
			}
			break;
		default:
			strcpy(mojiretu, "間違えないで（0：グー，1：チョキ，2：パー）やで");
			break;
		}
		switch (obj->zyanken) {
		case 0:
			strcpy(buf1, "グー");
			break;
		case 1:
			strcpy(buf1, "チョキ");
			break;
		case 2:
			strcpy(buf1, "パー");
			break;
		}
		switch (clientzyanken) {
		case 0:
			strcpy(buf2, "グー");
			break;
		case 1:
			strcpy(buf2, "チョキ");
			break;
		case 2:
			strcpy(buf2, "パー");
			break;
		}
		sprintf(buf, "コンピュータは %s あなたは %s \r\n", buf1, buf2, clientzyanken);
		tcpsnd(obj->clientsocket, buf); // 応答メッセージ送信
		sprintf(buf, "%s　%d勝%d負%d分け \r\n", mojiretu, obj->make, obj->kati, obj->wake);
		tcpsnd(obj->clientsocket, buf); // 応答メッセージ送信
		i++;
	}
	EnterCriticalSection(&g_sect); // 排他制御開始
								   // 該当クライアント情報をリスト構造から削除
	for (ite = g_clientlist.begin(); ite != g_clientlist.end(); ite++) {
		tempobj = *ite;
		if (tempobj->clientaddr.sin_addr.S_un.S_addr == obj->clientaddr.sin_addr.S_un.S_addr) {
			if (tempobj->clientaddr.sin_port == obj->clientaddr.sin_port) {
				g_clientlist.erase(ite);
				break;
			}
		}
	}
	LeaveCriticalSection(&g_sect); // 排他制御終了
	shutdown(obj->clientsocket, SD_BOTH);
	closesocket(obj->clientsocket);
	delete obj;
	printf("クライアントサブ　スレッド終了\n");
	_endthread();
}
//////////////////////////////////////////////////////////////////
// クライアントからの接続要求を受け付けるためのスレッド //
// 引数 //
// p1 : ＮＵＬＬ //
// 戻り値 //
// なし //
//////////////////////////////////////////////////////////////////
void acceptsub(void* p1)
{
	ClientInfo* clientobj; // クライアントの情報を格納するためのエリア
	printf("アクセプト処理 開始\n");
	g_ite = g_clientlist.begin(); // リスト構造の先頭を取得
	ClientInfo* tempobj; // クライアント情報格納用
	while (1) {
		// クライアント情報を生成
		clientobj = new ClientInfo();
		// クライアントからの接続要求を受け付ける
		clientobj->clientsocket = accept(g_mysock, (sockaddr*)&clientobj->clientaddr, &clientobj
			->clientaddrlength);
		// エラーが発生した場合には、ループを抜ける
		if (clientobj->clientsocket == INVALID_SOCKET) {
			delete clientobj;
			break;
		}
		// クライアントからの接続要求を処理するためのスレッドをスタートさせる
		_beginthread(clientsub, 400, clientobj);
		// クライアント情報をリスト構造にセットするため、排他制御開始
		EnterCriticalSection(&g_sect);
		// クライアント情報をリスト構造につなぐ
		g_clientlist.insert(g_ite, clientobj);
		printf("リスト数 %d \n", g_clientlist.size());
		// 排他制御終了
		LeaveCriticalSection(&g_sect);
	}
	EnterCriticalSection(&g_sect); // 排他制御開始
								   // すべてのクライアントについてソケット終了処理を行う
	for (g_ite = g_clientlist.begin(); g_ite != g_clientlist.end(); g_ite++) {
		tempobj = *g_ite;
		shutdown(tempobj->clientsocket, SD_BOTH);
		closesocket(tempobj->clientsocket);
	}
	LeaveCriticalSection(&g_sect); // 排他制御終了
	printf("アクセプト処理 終了\n");
	_endthread();
}
/////////////////////////////////
// メイン関数 //
/////////////////////////////////
void main()
{
	WSADATA wd;
	int y;
	// クリティカルセクションを生成する。
	InitializeCriticalSection(&g_sect);
	// WINSOCK初期処理
	WSAStartup(MAKEWORD(1, 1), &wd);
	// ＩＰアドレスを表示する
	getip();
	// ソケット生成
	g_mysock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	// ソケットにＩＰアドレス、ポート番号をセットする
	g_myaddr.sin_family = AF_INET;
	g_myaddr.sin_port = htons(g_myportno);
	g_myaddr.sin_addr.s_addr = INADDR_ANY;
	bind(g_mysock, (LPSOCKADDR)&g_myaddr, sizeof(SOCKADDR_IN));
	// 接続要求受付開始
	listen(g_mysock, SOMAXCONN);
	// 接続要求を受けるためのスレッドを開始
	_beginthread(acceptsub, 100, NULL);
	// 終了を待つ。
	while (1) {
		printf("終了させる時はYと入力してください。\n");
		y = getch();
		if (y == 'Y') {
			break;
		}
	}
	// シャットダウン処理
	shutdown(g_mysock, SD_BOTH);
	// ソケットをクローズさせる
	closesocket(g_mysock);
	// WINSOCK終了処理
	WSACleanup();
	// クリティカルセクションを削除する
	DeleteCriticalSection(&g_sect);
}