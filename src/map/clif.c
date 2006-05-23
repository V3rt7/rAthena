// Copyright (c) Athena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#define DUMP_UNKNOWN_PACKET	0
#define DUMP_ALL_PACKETS	0

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#ifdef __WIN32
#define __USE_W32_SOCKETS
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <time.h>

#include "../common/socket.h"
#include "../common/timer.h"
#include "../common/malloc.h"
#include "../common/version.h"
#include "../common/nullpo.h"
#include "../common/showmsg.h"

#include "map.h"
#include "chrif.h"
#include "clif.h"
#include "pc.h"
#include "status.h"
#include "npc.h"
#include "itemdb.h"
#include "chat.h"
#include "trade.h"
#include "storage.h"
#include "script.h"
#include "skill.h"
#include "atcommand.h"
#include "charcommand.h"
#include "intif.h"
#include "battle.h"
#include "mob.h"
#include "party.h"
#include "unit.h"
#include "guild.h"
#include "vending.h"
#include "pet.h"
#include "log.h"

#include "irc.h"

struct Clif_Config {
	int packet_db_ver;	//Preferred packet version.
	int connect_cmd[MAX_PACKET_VER + 1]; //Store the connect command for all versions. [Skotlex]
} clif_config;

struct packet_db packet_db[MAX_PACKET_VER + 1][MAX_PACKET_DB];

static const int packet_len_table[MAX_PACKET_DB] = {
   10,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,
//#0x0040
    0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0, 55, 17,  3, 37,  46, -1, 23, -1,  3,108,  3,  2,
#if PACKETVER < 2
    3, 28, 19, 11,  3, -1,  9,  5,  52, 51, 56, 58, 41,  2,  6,  6,
#else	// 78-7b 亀島以降 lv99エフェクト用
    3, 28, 19, 11,  3, -1,  9,  5,  54, 53, 58, 60, 41,  2,  6,  6,
#endif
//#0x0080
    7,  3,  2,  2,  2,  5, 16, 12,  10,  7, 29,  2, -1, -1, -1,  0, // 0x8b changed to 2 (was 23)
    7, 22, 28,  2,  6, 30, -1, -1,   3, -1, -1,  5,  9, 17, 17,  6,
   23,  6,  6, -1, -1, -1, -1,  8,   7,  6,  7,  4,  7,  0, -1,  6,
    8,  8,  3,  3, -1,  6,  6, -1,   7,  6,  2,  5,  6, 44,  5,  3,
//#0x00C0
    7,  2,  6,  8,  6,  7, -1, -1,  -1, -1,  3,  3,  6,  3,  2, 27, // 0xcd change to 3 (was 6)
    3,  4,  4,  2, -1, -1,  3, -1,   6, 14,  3, -1, 28, 29, -1, -1,
   30, 30, 26,  2,  6, 26,  3,  3,   8, 19,  5,  2,  3,  2,  2,  2,
    3,  2,  6,  8, 21,  8,  8,  2,   2, 26,  3, -1,  6, 27, 30, 10,

//#0x0100
    2,  6,  6, 30, 79, 31, 10, 10,  -1, -1,  4,  6,  6,  2, 11, -1,
   10, 39,  4, 10, 31, 35, 10, 18,   2, 13, 15, 20, 68,  2,  3, 16,
    6, 14, -1, -1, 21,  8,  8,  8,   8,  8,  2,  2,  3,  4,  2, -1,
    6, 86,  6, -1, -1,  7, -1,  6,   3, 16,  4,  4,  4,  6, 24, 26,
//#0x0140
   22, 14,  6, 10, 23, 19,  6, 39,   8,  9,  6, 27, -1,  2,  6,  6,
  110,  6, -1, -1, -1, -1, -1,  6,  -1, 54, 66, 54, 90, 42,  6, 42,
   -1, -1, -1, -1, -1, 30, -1,  3,  14,  3, 30, 10, 43, 14,186,182,
   14, 30, 10,  3, -1,  6,106, -1,   4,  5,  4, -1,  6,  7, -1, -1,
//#0x0180
    6,  3,106, 10, 10, 34,  0,  6,   8,  4,  4,  4, 29, -1, 10,  6,
#if PACKETVER < 1
   90, 86, 24,  6, 30,102,  8,  4,   8,  4, 14, 10, -1,  6,  2,  6,
#else	// 196 comodo以降 状態表示アイコン用
   90, 86, 24,  6, 30,102,  9,  4,   8,  4, 14, 10, -1,  6,  2,  6,
#endif
    3,  3, 35,  5, 11, 26, -1,  4,   4,  6, 10, 12,  6, -1,  4,  4,
   11,  7, -1, 67, 12, 18,114,  6,   3,  6, 26, 26, 26, 26,  2,  3,
//#0x01C0,   Set 0x1d5=-1
    2, 14, 10, -1, 22, 22,  4,  2,  13, 97,  3,  9,  9, 30,  6, 28,
    8, 14, 10, 35,  6, -1,  4, 11,  54, 53, 60,  2, -1, 47, 33,  6,
   30,  8, 34, 14,  2,  6, 26,  2,  28, 81,  6, 10, 26,  2, -1, -1,
   -1, -1, 20, 10, 32,  9, 34, 14,   2,  6, 48, 56, -1,  4,  5, 10,
//#0x200
   26, -1,  26, 10, 18, 26, 11, 34,  14, 36, 10, 0,  0, -1, 32, 10, // 0x20c change to 0 (was 19)
   22,  0,  26, 26, 42, -1, -1,  2,   2,282,282,10, 10, -1, -1, 66,
   10, -1,  -1,  8, 10,  2,282, 18,  18, 15, 58, 57, 64, 5, 69,  5,
   12, 26,   9, 11, -1, -1, 10,  2, 282, 11,  4, 36, -1,-1,  4,  2,
   -1, -1,  -1, -1, -1,  3,  4,  8,  -1,  3, 70,  4,  8,12,  4, 10,
    3, 32,  -1,  3,  3,  5,  5,  8,   2,  3, -1, -1,  4,-1,  4
};

// local define
enum {
	ALL_CLIENT,
	ALL_SAMEMAP,
	AREA,
	AREA_WOS,
	AREA_WOC,
	AREA_WOSC,
	AREA_CHAT_WOC,
	CHAT,
	CHAT_WOS,
	CHAT_MAINCHAT,
	PARTY,
	PARTY_WOS,
	PARTY_SAMEMAP,
	PARTY_SAMEMAP_WOS,
	PARTY_AREA,
	PARTY_AREA_WOS,
	GUILD,
	GUILD_WOS,
	GUILD_SAMEMAP,	// [Valaris]
	GUILD_SAMEMAP_WOS,
	GUILD_AREA,
	GUILD_AREA_WOS,	// end additions [Valaris]
	SELF
};

//Converts item type in case of pet eggs.
#define itemtype(a) (a == 7)?4:a

#define WBUFPOS(p,pos,x,y) { unsigned char *__p = (p); __p+=(pos); __p[0] = (x)>>2; __p[1] = ((x)<<6) | (((y)>>4)&0x3f); __p[2] = (y)<<4; }
#define WBUFPOS2(p,pos,x0,y0,x1,y1) { unsigned char *__p = (p); __p+=(pos); __p[0] = (unsigned char)((x0)>>2); __p[1] = (unsigned char)(((x0)<<6) | (((y0)>>4)&0x3f)); __p[2] = (unsigned char)(((y0)<<4) | (((x1)>>6)&0x0f)); __p[3]=(unsigned char)(((x1)<<2) | (((y1)>>8)&0x03)); __p[4]=(unsigned char)(y1); }

#define WFIFOPOS(fd,pos,x,y) { WBUFPOS (WFIFOP(fd,pos),0,x,y); }
#define WFIFOPOS2(fd,pos,x0,y0,x1,y1) { WBUFPOS2(WFIFOP(fd,pos),0,x0,y0,x1,y1); }

//To make the assignation of the level based on limits clearer/easier. [Skotlex]
#define clif_setlevel(lv) (lv<battle_config.max_lv?lv:battle_config.max_lv-(lv<battle_config.aura_lv?1:0));

//Removed sd->npc_shopid because there is no packet sent from the client when you cancel a buy!
//Quick check to know if the player shouldn't be "busy" with something else to deny action requests. [Skotlex]
#define clif_cant_act(sd) (sd->npc_id || sd->vender_id || sd->chatID || sd->sc.opt1 || sd->state.trading || sd->state.storage_flag)

// Checks if SD is in a trade/shop (where messing with the inventory can cause problems/exploits)
#define clif_trading(sd) (sd->npc_id || sd->vender_id || sd->state.trading )

//To idenfity disguised characters.
#define disguised(bl) (bl->type==BL_PC && ((TBL_PC*)bl)->disguise)
static char map_ip_str[16];
static in_addr_t map_ip;
static in_addr_t bind_ip = INADDR_ANY;
static int map_port = 5121;
int map_fd;
char talkie_mes[MESSAGE_SIZE];

//These two will be used to verify the incoming player's validity.
//It helps identify their client packet version correctly. [Skotlex]
static int max_account_id = DEFAULT_MAX_ACCOUNT_ID;
static int max_char_id = DEFAULT_MAX_CHAR_ID;

int clif_parse (int fd);
static void clif_hpmeter_single(int fd, struct map_session_data *sd);

/*==========================================
 * map鯖のip設定
 *------------------------------------------
 */
void clif_setip(char *ip)
{
	memcpy(map_ip_str, ip, 16);
	map_ip = inet_addr(map_ip_str);
}

void clif_setbindip(char *ip)
{
	bind_ip = inet_addr(ip);
}

/*==========================================
 * map鯖のport設定
 *------------------------------------------
 */
void clif_setport(int port)
{
	map_port = port;
}

/*==========================================
 * map鯖のip読み出し
 *------------------------------------------
 */
in_addr_t clif_getip(void)
{
	return map_ip;
}

/*==========================================
 * map鯖のport読み出し
 *------------------------------------------
 */
int clif_getport(void)
{
	return map_port;
}

/*==========================================
 * Counts connected players.
 *------------------------------------------
 */
int clif_countusers(void)
{
	int users = 0, i;
	struct map_session_data *sd;

	for(i = 0; i < fd_max; i++) {
		if (session[i] && (sd = (struct map_session_data*)session[i]->session_data) && sd->state.auth &&
				!(battle_config.hide_GM_session && pc_isGM(sd)))
			users++;
	}
	return users;
}

/*==========================================
 * 全てのclientに対してfunc()実行
 *------------------------------------------
 */

int clif_foreachclient(int (*func)(struct map_session_data*, va_list),...) //recoded by sasuke, bug when player count gets higher [Kevin]
{
	int i;
	va_list ap;
	struct map_session_data *sd;

	va_start(ap,func);

	for(i = 0; i < fd_max; i++) {
		if ( session[i] ) {
			sd = (struct map_session_data*)session[i]->session_data;
			if ( sd && session[i]->func_parse == clif_parse &&
					sd->state.auth && !sd->state.waitingdisconnect )
				func(sd, ap);
		}
	}

	va_end(ap);
	return 0;
}

/*==========================================
 * clif_sendでAREA*指定時用
 *------------------------------------------
 */
int clif_send_sub(struct block_list *bl, va_list ap)
{
	struct block_list *src_bl;
	struct map_session_data *sd;
	unsigned char *buf;
	int len, type;

	nullpo_retr(0, bl);
	nullpo_retr(0, ap);
	nullpo_retr(0, sd = (struct map_session_data *)bl);

	if (!sd->fd) //Avoid attempting to send to disconnected chars (may prevent buffer overrun errors?) [Skotlex]
		return 0;

	buf = va_arg(ap,unsigned char*);
	len = va_arg(ap,int);
	nullpo_retr(0, src_bl = va_arg(ap,struct block_list*));
	type = va_arg(ap,int);

	switch(type) {
	case AREA_WOS:
		if (bl == src_bl)
			return 0;
		break;
	case AREA_WOC:
		if (sd->chatID || bl == src_bl)
			return 0;
		break;
	case AREA_WOSC:
		{
			struct map_session_data *ssd = (struct map_session_data *)src_bl;
			if ((ssd != 0) && (src_bl->type == BL_PC) && (sd->chatID != 0) && (sd->chatID == ssd->chatID))
				return 0;
		}
		break;
	}

	if (session[sd->fd] != NULL) {
		WFIFOHEAD(sd->fd, len);
		if (WFIFOP(sd->fd,0) == buf) {
			printf("WARNING: Invalid use of clif_send function\n");
			printf("         Packet x%4x use a WFIFO of a player instead of to use a buffer.\n", WBUFW(buf,0));
			printf("         Please correct your code.\n");
			// don't send to not move the pointer of the packet for next sessions in the loop
		} else {
			if (packet_db[sd->packet_ver][RBUFW(buf,0)].len) { // packet must exist for the client version
				memcpy(WFIFOP(sd->fd,0), buf, len);
				//Check if hidden, better to modify the char's buffer than the
				//given buffer to prevent intravision affecting the packet as 
				//it's being received by everyone. [Skotlex]
				if ((sd->special_state.intravision || sd->sc.data[SC_INTRAVISION].timer != -1 )
						&& bl != src_bl && WFIFOW(sd->fd,0) == 0x0196)
				{	//New intravision method, just modify the status change/start packet. [Skotlex]
					switch (WFIFOW(sd->fd,2)) {
						case SI_HIDING:
						case SI_CLOAKING:
						case SI_CHASEWALK:
							WFIFOW(sd->fd,2) = SI_INTRAVISION;
					}
				}

				/* Previous implementation.
					if ((sd->special_state.intravision || sd->sc.data[SC_INTRAVISION].timer != -1 ) && bl != src_bl) {

					struct status_change *sc = status_get_sc(src_bl);
					if(sc && (sc->option&(OPTION_HIDE|OPTION_CLOAK)))
					{	//optionの修正
						switch(((unsigned short*)buf)[0])
						{
							case 0x119:
								WFIFOW(sd->fd,10) &= ~(OPTION_HIDE|OPTION_CLOAK);
								break;
#if PACKETVER < 4
							case 0x78:
#else
							case 0x1da:
#endif
							case 0x7b:
							case 0x7c:
							case 0x1d8:
								WFIFOW(sd->fd,12) &=~(OPTION_HIDE|OPTION_CLOAK);
								break;
						}
					}
				}
				*/
				WFIFOSET(sd->fd,len);
			}
		}
	}

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_send (unsigned char *buf, int len, struct block_list *bl, int type) {
	int i;
	struct map_session_data *sd = NULL;
	struct party *p = NULL;
	struct guild *g = NULL;
	int x0 = 0, x1 = 0, y0 = 0, y1 = 0;

	if (type != ALL_CLIENT &&
		type != CHAT_MAINCHAT) {
		nullpo_retr(0, bl);
		if (bl->type == BL_PC) {
			sd = (struct map_session_data *)bl;
		}
	}

	switch(type) {
	case ALL_CLIENT: // 全クライアントに送信
		for (i = 0; i < fd_max; i++) {
			if (session[i] && (sd = (struct map_session_data *)session[i]->session_data) != NULL && sd->state.auth) {
				if (packet_db[sd->packet_ver][RBUFW(buf,0)].len) { // packet must exist for the client version
					WFIFOHEAD(i, len);
					memcpy(WFIFOP(i,0), buf, len);
					WFIFOSET(i,len);
				}
			}
		}
		break;
	case ALL_SAMEMAP: // 同じマップの全クライアントに送信
		for(i = 0; i < fd_max; i++) {
			if (session[i] && (sd = (struct map_session_data*)session[i]->session_data) != NULL &&
				sd->state.auth && sd->bl.m == bl->m) {
				if (packet_db[sd->packet_ver][RBUFW(buf,0)].len) { // packet must exist for the client version
					WFIFOHEAD(i,len);
					memcpy(WFIFOP(i,0), buf, len);
					WFIFOSET(i,len);
				}
			}
		}
		break;
	case AREA:
	case AREA_WOS:
	case AREA_WOC:
	case AREA_WOSC:
		map_foreachinarea(clif_send_sub, bl->m, bl->x-AREA_SIZE, bl->y-AREA_SIZE, bl->x+AREA_SIZE, bl->y+AREA_SIZE,
			BL_PC, buf, len, bl, type);
		break;
	case AREA_CHAT_WOC:
		map_foreachinarea(clif_send_sub, bl->m, bl->x-(AREA_SIZE-5), bl->y-(AREA_SIZE-5),
			bl->x+(AREA_SIZE-5), bl->y+(AREA_SIZE-5), BL_PC, buf, len, bl, AREA_WOC);
		break;
	case CHAT:
	case CHAT_WOS:
		{
			struct chat_data *cd;
			if (sd) {
				cd = (struct chat_data*)map_id2bl(sd->chatID);
			} else if (bl->type == BL_CHAT) {
				cd = (struct chat_data*)bl;
			} else break;
			if (cd == NULL)
				break;
			for(i = 0; i < cd->users; i++) {
				if (type == CHAT_WOS && cd->usersd[i] == sd)
					continue;
				if (packet_db[cd->usersd[i]->packet_ver][RBUFW(buf,0)].len) { // packet must exist for the client version
					if (cd->usersd[i]->fd >0 && session[cd->usersd[i]->fd]) // Added check to see if session exists [PoW]
					{
						WFIFOHEAD(cd->usersd[i]->fd,len);
						memcpy(WFIFOP(cd->usersd[i]->fd,0), buf, len);
						WFIFOSET(cd->usersd[i]->fd,len);
					}
				}
			}
		}
		break;
	case CHAT_MAINCHAT: //[LuzZza]
		for(i=1; i<fd_max; i++) {
			if(session[i] && (sd = (struct map_session_data*)session[i]->session_data) != NULL &&
				sd->state.mainchat && sd->fd) {
					WFIFOHEAD(sd->fd, len);								
					memcpy(WFIFOP(sd->fd,0), buf, len);
					WFIFOSET(sd->fd, len);								
			}
		}
		break;
	case PARTY_AREA:		// 同じ画面内の全パーティーメンバに送信
	case PARTY_AREA_WOS:	// 自分以外の同じ画面内の全パーティーメンバに送信
		x0 = bl->x - AREA_SIZE;
		y0 = bl->y - AREA_SIZE;
		x1 = bl->x + AREA_SIZE;
		y1 = bl->y + AREA_SIZE;
	case PARTY:				// 全パーティーメンバに送信
	case PARTY_WOS:			// 自分以外の全パーティーメンバに送信
	case PARTY_SAMEMAP:		// 同じマップの全パーティーメンバに送信
	case PARTY_SAMEMAP_WOS:	// 自分以外の同じマップの全パーティーメンバに送信
		if (sd && sd->status.party_id)
			p = party_search(sd->status.party_id);
			
		if (p) {
			for(i=0;i<MAX_PARTY;i++){
				if ((sd = p->member[i].sd) != NULL) {
					if (!sd->fd || session[sd->fd] == NULL || sd->state.auth == 0
						|| session[sd->fd]->session_data == NULL || sd->packet_ver > MAX_PACKET_VER)
						continue;
					
					if (sd->bl.id == bl->id && (type == PARTY_WOS || type == PARTY_SAMEMAP_WOS || type == PARTY_AREA_WOS))
						continue;
					
					if (type != PARTY && type != PARTY_WOS && bl->m != sd->bl.m) // マップチェック
						continue;
					
					if ((type == PARTY_AREA || type == PARTY_AREA_WOS) &&
						(sd->bl.x < x0 || sd->bl.y < y0 || sd->bl.x > x1 || sd->bl.y > y1))
						continue;
					
					if (packet_db[sd->packet_ver][RBUFW(buf,0)].len) { // packet must exist for the client version
						WFIFOHEAD(sd->fd,len);
						memcpy(WFIFOP(sd->fd,0), buf, len);
						WFIFOSET(sd->fd,len);
					}
				}
			}
			if (!enable_spy) //Skip unnecessary parsing. [Skotlex]
				break;
			for (i = 1; i < fd_max; i++){ // partyspy [Syrus22]
				if (session[i] && (sd = (struct map_session_data*)session[i]->session_data) != NULL && sd->state.auth && sd->fd && sd->partyspy) {
					if (sd->partyspy == p->party_id) {
						if (sd->fd && packet_db[sd->packet_ver][RBUFW(buf,0)].len) { // packet must exist for the client version
							WFIFOHEAD(sd->fd,len);
							memcpy(WFIFOP(sd->fd,0), buf, len);
							WFIFOSET(sd->fd,len);
						}
					}
				}
			}
		}
		break;
	case SELF:
		if (sd && sd->fd && packet_db[sd->packet_ver][RBUFW(buf,0)].len) { // packet must exist for the client version
			WFIFOHEAD(sd->fd,len);
			memcpy(WFIFOP(sd->fd,0), buf, len);
			WFIFOSET(sd->fd,len);
		}
		break;

// New definitions for guilds [Valaris] - Cleaned up and reorganized by [Skotlex]
	case GUILD_AREA:
	case GUILD_AREA_WOS:
		x0 = bl->x - AREA_SIZE;
		y0 = bl->y - AREA_SIZE;
		x1 = bl->x + AREA_SIZE;
		y1 = bl->y + AREA_SIZE;
	case GUILD_SAMEMAP:
	case GUILD_SAMEMAP_WOS:
	case GUILD:
	case GUILD_WOS:
		if (sd && sd->status.guild_id)
			g = guild_search(sd->status.guild_id);

		if (g) {
			for(i = 0; i < g->max_member; i++) {
				if ((sd = g->member[i].sd) != NULL) {
					if (!sd->fd || session[sd->fd] == NULL || sd->state.auth == 0
						|| session[sd->fd]->session_data == NULL || sd->packet_ver > MAX_PACKET_VER)
						continue;
					
					if (sd->bl.id == bl->id && (type == GUILD_WOS || type == GUILD_SAMEMAP_WOS || type == GUILD_AREA_WOS))
						continue;
					
					if (type != GUILD && type != GUILD_WOS && sd->bl.m != bl->m)
						continue;
					
					if ((type == GUILD_AREA || type == GUILD_AREA_WOS) &&
						(sd->bl.x < x0 || sd->bl.y < y0 || sd->bl.x > x1 || sd->bl.y > y1))
						continue;

					if (packet_db[sd->packet_ver][RBUFW(buf,0)].len) { // packet must exist for the client version
						WFIFOHEAD(sd->fd,len);
						memcpy(WFIFOP(sd->fd,0), buf, len);
						WFIFOSET(sd->fd,len);
					}
				}
			}
			if (!enable_spy) //Skip unnecessary parsing. [Skotlex]
				break;
			for (i = 1; i < fd_max; i++){ // guildspy [Syrus22]
				if (session[i] && (sd = (struct map_session_data*)session[i]->session_data) != NULL && sd->state.auth && sd->fd && sd->guildspy) {
					if (sd->guildspy == g->guild_id) {
						if (packet_db[sd->packet_ver][RBUFW(buf,0)].len) { // packet must exist for the client version
							WFIFOHEAD(sd->fd,len);
							memcpy(WFIFOP(sd->fd,0), buf, len);
							WFIFOSET(sd->fd,len);
						}
					}
				}
			}
		}
		break;
/*				End [Valaris]			*/

	default:
		if (battle_config.error_log)
			ShowError("clif_send: Unrecognized type %d\n",type);
		return -1;
	}

	return 0;
}

//
// パケット作って送信
//
/*==========================================
 *
 *------------------------------------------
 */
int clif_authok(struct map_session_data *sd) {
	int fd;

	if (!sd->fd)
		return 0;
	fd = sd->fd;

	WFIFOHEAD(fd, packet_len_table[0x73]);
	WFIFOW(fd, 0) = 0x73;
	WFIFOL(fd, 2) = gettick();
	WFIFOPOS(fd, 6, sd->bl.x, sd->bl.y);
	WFIFOB(fd, 9) = 5;
	WFIFOB(fd,10) = 5;
	WFIFOSET(fd,packet_len_table[0x73]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_authfail_fd(int fd, int type) {
	if (!fd || !session[fd] || session[fd]->func_parse != clif_parse) //clif_authfail should only be invoked on players!
		return 0;

	WFIFOHEAD(fd, packet_len_table[0x81]);
	WFIFOW(fd,0) = 0x81;
	WFIFOB(fd,2) = type;
	WFIFOSET(fd,packet_len_table[0x81]);
	clif_setwaitclose(fd);
	return 0;
}

/*==========================================
 * Used to know which is the max valid account/char id [Skotlex]
 *------------------------------------------
 */
void clif_updatemaxid(int account_id, int char_id)
{
	max_account_id = account_id;
	max_char_id = char_id;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_charselectok(int id) {
	struct map_session_data *sd;
	int fd;

	if ((sd = map_id2sd(id)) == NULL)
		return 1;

	if (!sd->fd)
		return 1;

	fd = sd->fd;
	WFIFOHEAD(fd, packet_len_table[0xb3]);
	WFIFOW(fd,0) = 0xb3;
	WFIFOB(fd,2) = 1;
	WFIFOSET(fd,packet_len_table[0xb3]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
static int clif_set009e(struct flooritem_data *fitem,unsigned char *buf) {
	int view;

	nullpo_retr(0, fitem);

	//009e <ID>.l <name ID>.w <identify flag>.B <X>.w <Y>.w <subX>.B <subY>.B <amount>.w
	WBUFW(buf, 0) = 0x9e;
	WBUFL(buf, 2) = fitem->bl.id;
	if ((view = itemdb_viewid(fitem->item_data.nameid)) > 0)
		WBUFW(buf, 6) = view;
	else
		WBUFW(buf, 6) = fitem->item_data.nameid;
	WBUFB(buf, 8) = fitem->item_data.identify;
	WBUFW(buf, 9) = fitem->bl.x;
	WBUFW(buf,11) = fitem->bl.y;
	WBUFB(buf,13) = fitem->subx;
	WBUFB(buf,14) = fitem->suby;
	WBUFW(buf,15) = fitem->item_data.amount;

	return packet_len_table[0x9e];
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_dropflooritem(struct flooritem_data *fitem) {
	unsigned char buf[64];

	nullpo_retr(0, fitem);

	if (fitem->item_data.nameid <= 0)
		return 0;
	clif_set009e(fitem, buf);
	clif_send(buf, packet_len_table[0x9e], &fitem->bl, AREA);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_clearflooritem(struct flooritem_data *fitem, int fd) {
	unsigned char buf[16];

	nullpo_retr(0, fitem);

	WBUFW(buf,0) = 0xa1;
	WBUFL(buf,2) = fitem->bl.id;

	if (fd == 0) {
		clif_send(buf, packet_len_table[0xa1], &fitem->bl, AREA);
	} else {
		WFIFOHEAD(fd,packet_len_table[0xa1]);
		memcpy(WFIFOP(fd,0), buf, 6);
		WFIFOSET(fd,packet_len_table[0xa1]);
	}

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_clearchar(struct block_list *bl, int type) {
	unsigned char buf[16];

	nullpo_retr(0, bl);

	WBUFW(buf,0) = 0x80;
	WBUFL(buf,2) = bl->id;
	WBUFB(buf,6) = type;

	clif_send(buf, packet_len_table[0x80], bl, type == 1 ? AREA : AREA_WOS);
	if(disguised(bl)) {
		WBUFL(buf,2) = -bl->id;
		clif_send(buf, packet_len_table[0x80], bl, SELF);
	}

	return 0;
}

static int clif_clearchar_delay_sub(int tid, unsigned int tick, int id, int data) {
	struct block_list *bl = (struct block_list *)id;

	clif_clearchar(bl,data);
	aFree(bl);
	return 0;
}

int clif_clearchar_delay(unsigned int tick, struct block_list *bl, int type) {
	struct block_list *tbl;
	tbl = aMalloc(sizeof (struct block_list));
	memcpy (tbl, bl, sizeof (struct block_list));
	add_timer(tick, clif_clearchar_delay_sub, (int)tbl, type);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_clearchar_id(int id, int type, int fd) {

	WFIFOHEAD(fd, packet_len_table[0x80]);
	WFIFOW(fd,0)	= 0x80;
	WFIFOL(fd,2)	= id;
	WFIFOB(fd,6)	= (char)type; // Why use int for a char in the first place?
	WFIFOSET(fd, packet_len_table[0x80]);

	return 0;
}

void clif_get_weapon_view(TBL_PC* sd, unsigned short *rhand, unsigned short *lhand)
{
#if PACKETVER > 3
	struct item_data *id;
#endif

	if(sd->sc.option&OPTION_WEDDING || sd->vd.class_ == JOB_XMAS)
	{
		*rhand = *lhand = 0;
		return;
	}

#if PACKETVER < 4
	*rhand = sd->status.weapon;
	*lhand = sd->status.shield;
#else
	if (sd->equip_index[9] >= 0 && sd->inventory_data[sd->equip_index[9]]) 
	{
		id = sd->inventory_data[sd->equip_index[9]];
		if (id->view_id > 0)
			*rhand = id->view_id;
		else
			*rhand = id->nameid;
	} else
		*rhand = 0;

	if (sd->equip_index[8] >= 0 && sd->equip_index[8] != sd->equip_index[9]
			&& sd->inventory_data[sd->equip_index[8]]) 
	{
		id = sd->inventory_data[sd->equip_index[8]];
		if (id->view_id > 0)
			*lhand = id->view_id;
		else
			*lhand = id->nameid;
	} else
		*lhand = 0;
#endif
}	

static void clif_get_guild_data(struct block_list *bl, long *guild_id, short *emblem_id) 
{
	//TODO: There has to be a way to clean this up.
	switch (bl->type) {
		case BL_PC:
			*guild_id = ((TBL_PC*)bl)->status.guild_id;
			*emblem_id = ((TBL_PC*)bl)->guild_emblem_id;
			break;
		case BL_MOB:
			if (((TBL_MOB*)bl)->guardian_data) {
				*guild_id =((TBL_MOB*)bl)->guardian_data->guild_id;
				*emblem_id =((TBL_MOB*)bl)->guardian_data->emblem_id;
			}
			break;
		case BL_NPC:
			if (bl->subtype == SCRIPT && ((TBL_NPC*)bl)->u.scr.guild_id > 0) {
				struct guild *g = guild_search(((TBL_NPC*)bl)->u.scr.guild_id);
				if (g) {
					*guild_id =g->guild_id;
					*emblem_id =g->emblem_id;
				}
			}
			break;
		default:
			*guild_id = status_get_guild_id(bl);
	}
	return;
}
/*==========================================
 *
 *------------------------------------------
 */
static int clif_set0078(struct block_list *bl, struct view_data *vd, unsigned char *buf) {
	struct status_change *sc;
	struct map_session_data *sd;
	long guild_id=0;
	unsigned short emblem_id=0, lv;
	unsigned short dir;

	nullpo_retr(0, bl);
	BL_CAST(BL_PC, bl, sd);
	sc = status_get_sc(bl);

	clif_get_guild_data(bl, &guild_id, &emblem_id);
	dir = unit_getdir(bl);
	lv = status_get_lv(bl);
	if(pcdb_checkid(vd->class_)) { 
#if PACKETVER > 3
		memset(buf,0,packet_len_table[0x1d8]);

		WBUFW(buf,0)=0x1d8;
		WBUFL(buf,2)=bl->id;
		WBUFW(buf,6)=status_get_speed(bl);
		if (sc) {
			WBUFW(buf,8)=sc->opt1;
			WBUFW(buf,10)=sc->opt2;
			WBUFW(buf,12)=sc->option;
			WBUFW(buf,42)=sc->opt3;
		}
		WBUFW(buf,14)=vd->class_;
		WBUFW(buf,16)=vd->hair_style;
		WBUFW(buf,18)=vd->weapon;
		WBUFW(buf,20)=vd->shield;
		WBUFW(buf,22)=vd->head_bottom;
		WBUFW(buf,24)=vd->head_top;
		WBUFW(buf,26)=vd->head_mid;
		WBUFW(buf,28)=vd->hair_color;
		WBUFW(buf,30)=vd->cloth_color;
		WBUFW(buf,32)=sd?sd->head_dir:dir;
		WBUFL(buf,34)=guild_id;
		WBUFW(buf,38)=emblem_id;
		if (sd) {
			WBUFW(buf,40)=sd->status.manner;
			WBUFB(buf,44)=sd->status.karma;
		}
		WBUFB(buf,45)=vd->sex;
		WBUFPOS(buf,46,bl->x,bl->y);
		WBUFB(buf,48)|=dir & 0x0f;
		WBUFB(buf,49)=5;
		WBUFB(buf,50)=5;
		WBUFB(buf,51)=vd->dead_sit;
		WBUFW(buf,52)=clif_setlevel(lv);
		return packet_len_table[0x1d8];
#else
		memset(buf,0,packet_len_table[0x78]);

		WBUFW(buf,0)=0x78;
		WBUFL(buf,2)=bl->id;
		WBUFW(buf,6)=status_get_speed(bl);
		if (sc) {
			WBUFW(buf,8)=sc->opt1;
			WBUFW(buf,10)=sc->opt2;
			WBUFW(buf,12)=sc->option;
			WBUFW(buf,42)=sc->opt3;
		}
		WBUFW(buf,14)=vd->class_;
		WBUFW(buf,16)=vd->hair_style;
		WBUFW(buf,18)=vd->weapon;
		WBUFW(buf,20)=vd->head_bottom;
		WBUFW(buf,22)=vd->shield;
		WBUFW(buf,24)=vd->head_top;
		WBUFW(buf,26)=vd->head_mid;
		WBUFW(buf,28)=vd->hair_color;
		WBUFW(buf,30)=vd->cloth_color;
		WBUFW(buf,32)=sd?sd->head_dir:dir;
		WBUFL(buf,34)=guild_id;
		WBUFL(buf,38)=emblem_id;
		if (sd)
			WBUFB(buf,44)=sd->status.karma;
		WBUFB(buf,45)=vd->sex;
		WBUFPOS(buf,46,bl->x,bl->y);
		WBUFB(buf,48)|=dir&0x0f;
		WBUFB(buf,49)=5;
		WBUFB(buf,50)=5;
		WBUFB(buf,51)=vd->dead_sit;
		WBUFW(buf,52)=clif_setlevel(lv);
		return packet_len_table[0x78];
#endif
	}
	//Non-player sprites need just a few fields filled.
	memset(buf,0,packet_len_table[0x78]);

	WBUFW(buf,0)=0x78;
	WBUFL(buf,2)=bl->id;
	WBUFW(buf,6)=status_get_speed(bl);
	if (sc) {
		WBUFW(buf,8)=sc->opt1;
		WBUFW(buf,10)=sc->opt2;
		WBUFW(buf,12)=sc->option;
		WBUFW(buf,42)=sc->opt3;
	}
	WBUFW(buf,14)=vd->class_;
	WBUFW(buf,16)=vd->hair_style;  //Required for pets.
	WBUFW(buf,20)=vd->head_bottom;	//Pet armor
	if (bl->type == BL_NPC && vd->class_ == 722)
	{	//The hell, why flags work like this?
		WBUFL(buf,22)=emblem_id;
		WBUFL(buf,26)=guild_id;
	}
	WBUFW(buf,32)=dir;
	WBUFL(buf,34)=guild_id;
	WBUFL(buf,38)=emblem_id;
	WBUFPOS(buf,46,bl->x,bl->y);
	WBUFB(buf,48)|=dir&0x0f;
	WBUFB(buf,49)=5;
	WBUFB(buf,50)=5;
	WBUFW(buf,52)=clif_setlevel(lv);
	return packet_len_table[0x78];
}

/*==========================================
 *
 *------------------------------------------
 */
static int clif_set007b(struct block_list *bl, struct view_data *vd, struct unit_data *ud, unsigned char *buf) {
	struct status_change *sc;
	struct map_session_data *sd;
	long guild_id=0;
	unsigned short emblem_id=0, lv;

	nullpo_retr(0, bl);
	BL_CAST(BL_PC, bl, sd);
	sc = status_get_sc(bl);
	
	clif_get_guild_data(bl, &guild_id, &emblem_id);
	lv = status_get_lv(bl);
	
	if(pcdb_checkid(vd->class_)) { 
#if PACKETVER > 6
		memset(buf,0,packet_len_table[0x22c]);

		WBUFW(buf,0)=0x22c;
		WBUFL(buf,2)=bl->id;
		WBUFW(buf,6)=status_get_speed(bl);
		if (sc) {
			WBUFW(buf,8)= sc->opt1;
			WBUFW(buf,10)= sc->opt2;
			WBUFL(buf,12)= sc->option;
			WBUFL(buf,48)= sc->opt3;
		}
		WBUFW(buf,16)=vd->class_;
		WBUFW(buf,18)=vd->hair_style;
		WBUFW(buf,20)=vd->weapon;
		WBUFW(buf,22)=vd->shield;
		WBUFW(buf,24)=vd->head_bottom;
		WBUFL(buf,26)=gettick();
		WBUFW(buf,30)=vd->head_top;
		WBUFW(buf,32)=vd->head_mid;
		WBUFW(buf,34)=vd->hair_color;
		WBUFW(buf,36)=vd->cloth_color;
		WBUFW(buf,38)=sd?sd->head_dir:unit_getdir(bl);
		WBUFL(buf,40)=guild_id;
		WBUFW(buf,44)=emblem_id;
		if (sd) {
			WBUFW(buf,46)=sd->status.manner;
			WBUFB(buf,52)=sd->status.karma;
		}
		WBUFB(buf,53)=vd->sex;
		WBUFPOS2(buf,54,bl->x,bl->y,ud->to_x,ud->to_y);
		WBUFB(buf,59)=0x88; // Deals with acceleration in directions. [Valaris]
		WBUFB(buf,60)=0;
		WBUFB(buf,61)=0;
		WBUFW(buf,62)=clif_setlevel(lv);

		return packet_len_table[0x22c];	
#elif PACKETVER > 3
		memset(buf,0,packet_len_table[0x1da]);

		WBUFW(buf,0)=0x1da;
		WBUFL(buf,2)=bl->id;
		WBUFW(buf,6)=status_get_speed(bl);
		if (sc) {
			WBUFW(buf,8)=sc->opt1;
			WBUFW(buf,10)=sc->opt2;
			WBUFW(buf,12)=sc->option;
			WBUFW(buf,46)=sc->opt3;
		}
		WBUFW(buf,14)=vd->class_;
		WBUFW(buf,16)=vd->hair_style;
		WBUFW(buf,18)=vd->weapon;
		WBUFW(buf,20)=vd->shield;
		WBUFW(buf,22)=vd->head_bottom;
		WBUFL(buf,24)=gettick();
		WBUFW(buf,28)=vd->head_top;
		WBUFW(buf,30)=vd->head_mid;
		WBUFW(buf,32)=vd->hair_color;
		WBUFW(buf,34)=vd->cloth_color;
		WBUFW(buf,36)=sd?sd->head_dir:unit_getdir(bl);
		WBUFL(buf,38)=guild_id;
		WBUFW(buf,42)=emblem_id;
		if (sd) {
			WBUFW(buf,44)=sd->status.manner;
			WBUFB(buf,48)=sd->status.karma;
		}
		WBUFB(buf,49)=vd->sex;
		WBUFPOS2(buf,50,bl->x,bl->y,ud->to_x,ud->to_y);
		WBUFB(buf,55)=0x88; // Deals with acceleration in directions. [Valaris]
		WBUFB(buf,56)=5;
		WBUFB(buf,57)=5;
		WBUFW(buf,58)=clif_setlevel(lv);

		return packet_len_table[0x1da];
#else
		memset(buf,0,packet_len_table[0x7b]);

		WBUFW(buf,0)=0x7b;
		WBUFL(buf,2)=bl->id;
		WBUFW(buf,6)=status_get_speed(bl);
		if (sc) {
			WBUFW(buf,8)=sc->opt1;
			WBUFW(buf,10)=sc->opt2;
			WBUFW(buf,12)=sc->option;
			WBUFW(buf,46)=sc->opt3;
		}
		WBUFW(buf,14)=vd->class_;
		WBUFW(buf,16)=vd->hair_style;
		WBUFW(buf,18)=vd->weapon;
		WBUFW(buf,20)=vd->head_bottom;
		WBUFL(buf,22)=gettick();
		WBUFW(buf,26)=vd->shield;
		WBUFW(buf,28)=vd->head_top;
		WBUFW(buf,30)=vd->head_mid;
		WBUFW(buf,32)=vd->hair_color;
		WBUFW(buf,34)=vd->cloth_color;
		WBUFW(buf,36)=sd?sd->head_dir:unit_getdir(bl);
		WBUFL(buf,38)=guild_id;
		WBUFL(buf,42)=emblem_id;
		if (sd)
			WBUFB(buf,48)=sd->status.karma;
		WBUFB(buf,49)=vd->sex;
		WBUFPOS2(buf,50,bl->x,bl->y,ud->to_x,ud->to_y);
		WBUFB(buf,55)=0x88; // Deals with acceleration in directions. [Valaris]
		WBUFB(buf,56)=5;
		WBUFB(buf,57)=5;
		WBUFW(buf,58)=clif_setlevel(lv);

		return packet_len_table[0x7b];
#endif
	}
	//Non-player sprites only require a few fields.
#if PACKETVER > 6
	memset(buf,0,packet_len_table[0x22c]);

	WBUFW(buf,0)=0x22c;
	WBUFL(buf,2)=bl->id;
	WBUFW(buf,6)=status_get_speed(bl);
	if (sc) {
		WBUFW(buf,8)=sc->opt1;
		WBUFW(buf,10)=sc->opt2;
		WBUFL(buf,12)=sc->option;
		WBUFL(buf,48)=sc->opt3;
	}
	WBUFW(buf,16)=vd->class_;
	WBUFW(buf,18)=vd->hair_style; //For pets
	WBUFW(buf,24)=vd->head_bottom;	//Pet armor
	WBUFL(buf,26)=gettick();
	WBUFW(buf,38)=unit_getdir(bl);
	WBUFL(buf,40)=guild_id;
	WBUFL(buf,44)=emblem_id;
	WBUFPOS2(buf,54,bl->x,bl->y,ud->to_x,ud->to_y);
	WBUFB(buf,59)=0x88; // Deals with acceleration in directions. [Valaris]
	WBUFB(buf,60)=0;
	WBUFB(buf,61)=0;
	WBUFW(buf,62)=clif_setlevel(lv);
	return packet_len_table[0x22c];
#else
	memset(buf,0,packet_len_table[0x7b]);

	WBUFW(buf,0)=0x7b;
	WBUFL(buf,2)=bl->id;
	WBUFW(buf,6)=status_get_speed(bl);
	if (sc) {
		WBUFW(buf,8)=sc->opt1;
		WBUFW(buf,10)=sc->opt2;
		WBUFW(buf,12)=sc->option;
		WBUFW(buf,46)=sc->opt3;
	}
	WBUFW(buf,14)=vd->class_;
	WBUFW(buf,16)=vd->hair_style; //For pets
	WBUFW(buf,20)=vd->head_bottom;	//Pet armor
	WBUFL(buf,22)=gettick();
	WBUFW(buf,36)=unit_getdir(bl);
	WBUFL(buf,38)=guild_id;
	WBUFL(buf,42)=emblem_id;
	WBUFPOS2(buf,50,bl->x,bl->y,ud->to_x,ud->to_y);
	WBUFB(buf,55)=0x88; // Deals with acceleration in directions. [Valaris]
	WBUFB(buf,56)=5;
	WBUFB(buf,57)=5;
	WBUFW(buf,58)=clif_setlevel(lv);
	return packet_len_table[0x7b];
#endif
}

//Modifies the buffer for disguise characters and sends it to self.
//Flag = 0: change id to negative, buf will have disguise data.
//Flag = 1: change id to positive, class and option to make your own char invisible.
//Luckily, the offsets that need to be changed are the same in packets 0x78, 0x7b, 0x1d8 and 0x1da
//But no longer holds true for packet 0x22c
static void clif_setdisguise(struct map_session_data *sd, unsigned char *buf,int len, int flag) {

	if (flag) {
#if PACKETVER > 6
		if (WBUFW(buf,0)==0x22c) {
			WBUFL(buf,12)=OPTION_INVISIBLE;
			WBUFW(buf,16)=sd->status.class_;
		} else {
#endif
			WBUFL(buf,2)=sd->bl.id;
			WBUFW(buf,12)=OPTION_INVISIBLE;
			WBUFW(buf,14)=sd->status.class_;
#if PACKETVER > 6
		}
#endif
	} else {
		WBUFL(buf,2)=-sd->bl.id;
	}
	clif_send(buf, len, &sd->bl, SELF);
}

/*==========================================
 * クラスチェンジ typeはMobの場合は1で他は0？
 *------------------------------------------
 */
int clif_class_change(struct block_list *bl,int class_,int type)
{
	unsigned char buf[16];

	nullpo_retr(0, bl);

	if(!pcdb_checkid(class_)) {
		WBUFW(buf,0)=0x1b0;
		WBUFL(buf,2)=bl->id;
		WBUFB(buf,6)=type;
		WBUFL(buf,7)=class_;
		clif_send(buf,packet_len_table[0x1b0],bl,AREA);
	}
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_spiritball_single(int fd, struct map_session_data *sd)
{
	WFIFOHEAD(fd, packet_len_table[0x1e1]);
	WFIFOW(fd,0)=0x1e1;
	WFIFOL(fd,2)=sd->bl.id;
	WFIFOW(fd,6)=sd->spiritball;
	WFIFOSET(fd, packet_len_table[0x1e1]);
}

/*==========================================
 *
 *------------------------------------------
 */
static int clif_set0192(int fd, int m, int x, int y, int type) {
	WFIFOHEAD(fd, packet_len_table[0x192]);
	WFIFOW(fd,0) = 0x192;
	WFIFOW(fd,2) = x;
	WFIFOW(fd,4) = y;
	WFIFOW(fd,6) = type;
	memcpy(WFIFOP(fd,8),map[m].name,MAP_NAME_LENGTH);
	WFIFOSET(fd,packet_len_table[0x192]);

	return 0;
}

// new and improved weather display [Valaris]
static void clif_weather_sub(int fd, int id, int type) {
	WFIFOHEAD(fd, packet_len_table[0x1f3]);
	WFIFOW(fd,0) = 0x1f3;
	WFIFOL(fd,2) = id;
	WFIFOL(fd,6) = type;
	WFIFOSET(fd,packet_len_table[0x1f3]);
}

/*==========================================
 *
 *------------------------------------------
 */
static void clif_weather_check(struct map_session_data *sd) {
	int m = sd->bl.m, fd = sd->fd;
	
	if (map[m].flag.snow
		|| map[m].flag.clouds
		|| map[m].flag.fog
		|| map[m].flag.fireworks
		|| map[m].flag.sakura
		|| map[m].flag.leaves
		|| map[m].flag.rain
		|| map[m].flag.clouds2)
	{
		if (map[m].flag.snow)
			clif_weather_sub(fd, sd->bl.id, 162);
		if (map[m].flag.clouds)
			clif_weather_sub(fd, sd->bl.id, 233);
		if (map[m].flag.clouds2)
			clif_weather_sub(fd, sd->bl.id, 516);
		if (map[m].flag.fog)
			clif_weather_sub(fd, sd->bl.id, 515);
		if (map[m].flag.fireworks) {
			clif_weather_sub(fd, sd->bl.id, 297);
			clif_weather_sub(fd, sd->bl.id, 299);
			clif_weather_sub(fd, sd->bl.id, 301);
		}
		if (map[m].flag.sakura)
			clif_weather_sub(fd, sd->bl.id, 163);
		if (map[m].flag.leaves)
			clif_weather_sub(fd, sd->bl.id, 333);
		if (map[m].flag.rain)
			clif_weather_sub(fd, sd->bl.id, 161);
	}
}

int clif_weather(int m) {
	int i;

	struct map_session_data *sd=NULL;

	for(i = 0; i < fd_max; i++) {
		if (session[i] && (sd = session[i]->session_data) != NULL && sd->state.auth && sd->bl.m == m) {
			clif_weather_check(sd);
		}
	}

	return 0;
}

int clif_spawn(struct block_list *bl)
{
	unsigned char buf[128];
	struct view_data *vd;
	nullpo_retr(0, bl);
	vd = status_get_viewdata(bl);
	if (!vd || vd->class_ == INVISIBLE_CLASS)
		return 0;

	if (pcdb_checkid(vd->class_))
	{	//Player spawn packet.
		clif_set0078(bl, vd, buf);
#if PACKETVER > 3
		if (WBUFW(buf,0)==0x78) {
			WBUFW(buf, 0) = 0x79;
			WBUFW(buf,51) = WBUFW(buf,52); //Lv is placed on offset 52
			clif_send(buf, packet_len_table[0x79], bl, AREA_WOS);
			if (disguised(bl))
				clif_setdisguise((TBL_PC*)bl, buf, packet_len_table[0x79], 0);
		} else {
#endif
			WBUFW(buf, 0) = 0x1d9;
			WBUFW(buf,51) = WBUFW(buf,52); //Lv is placed on offset 52
			clif_send(buf, packet_len_table[0x1d9], bl, AREA_WOS);
			if (disguised(bl))
				clif_setdisguise((TBL_PC*)bl, buf, packet_len_table[0x1d9], 0);
#if PACKETVER > 3
		}
#endif

	} else {	//Mob spawn packet.
		struct status_change *sc = status_get_sc(bl);
		memset(buf,0,sizeof(buf));
		WBUFW(buf,0)=0x7c;
		WBUFL(buf,2)=bl->id;
		WBUFW(buf,6)=status_get_speed(bl);
		if (sc) {
			WBUFW(buf,8)=sc->opt1;
			WBUFW(buf,10)=sc->opt2;
			WBUFW(buf,12)=sc->option;
		}
		WBUFW(buf,20)=vd->class_;
		WBUFW(buf,22)=vd->hair_style;  //Required for pets.
		WBUFW(buf,24)=vd->head_bottom;	//Pet armor

		WBUFPOS(buf,36,bl->x,bl->y);
		clif_send(buf,packet_len_table[0x7c],bl,AREA_WOS);
		if (disguised(bl)) {
			WBUFL(buf,2)=-bl->id;
			clif_send(buf,packet_len_table[0x7c],bl,SELF);
		}
	}
	
	if (vd->cloth_color)
		clif_refreshlook(bl,bl->id,LOOK_CLOTHES_COLOR,vd->cloth_color,AREA_WOS);
		
	switch (bl->type)
	{
	case BL_PC:
		{
			TBL_PC *sd = ((TBL_PC*)bl);
			if (sd->spiritball > 0)
				clif_spiritball(sd);
			if(sd->state.size==2) // tiny/big players [Valaris]
				clif_specialeffect(bl,423,0);
			else if(sd->state.size==1)
				clif_specialeffect(bl,421,0);
		}
	break;
	case BL_MOB:
		{
			TBL_MOB *md = ((TBL_MOB*)bl);
			if(md->special_state.size==2) // tiny/big mobs [Valaris]
				clif_specialeffect(&md->bl,423,0);
			else if(md->special_state.size==1)
				clif_specialeffect(&md->bl,421,0);
		}
	break;
	}
	return 0;
}
/*==========================================
 * Homunculus [blackhole89]
 *------------------------------------------
 */
// Can somebody tell me why exactly I have commented this lot of stuff out?
// acknowledge client it has a homunculus
int clif_homunack(struct map_session_data *sd)
{
	struct homun_data *hd = sd->hd;
	unsigned char buf[64];

	nullpo_retr(0, sd);
	nullpo_retr(0, sd->hd);

	//memset(buf,0,packet_len_table[0x230]);
	memset(buf,0,12); //not yet set that stuff
	WBUFW(buf,0)=0x230;
	WBUFL(buf,4)=hd->bl.id;
	ShowError("in clif_homunack~\n");
	clif_send(buf,/*packet_len_table[0x230]*/12,&sd->bl,SELF);

	return 0;
}

// homunculus stats et al
int clif_homuninfo(struct map_session_data *sd)
{
	struct homun_data *hd = sd->hd;
	unsigned char buf[128];
	
	nullpo_retr(0, sd);
	nullpo_retr(0, sd->hd);

	memset(buf,0,71); //packet_len_table[0x22e]);
	WBUFW(buf,0)=0x22e;
	memcpy(WBUFP(buf,2),hd->name,NAME_LENGTH);
	WBUFW(buf,27)=hd->level;
	WBUFW(buf,29)=hd->hunger_rate;
	WBUFL(buf,31)=0xFF;	//intimacy, leave it as is
	WBUFW(buf,35)=hd->atk;
	WBUFW(buf,37)=hd->matk;
	WBUFW(buf,39)=hd->hit;
	WBUFW(buf,41)=hd->crit/10;	//crit is a +1 decimal value!
	WBUFW(buf,43)=hd->def;
	WBUFW(buf,45)=hd->mdef;
	WBUFW(buf,47)=hd->flee;
	WBUFW(buf,49)=status_get_amotion(&hd->bl)+200;	//credits to jA for this field.
	WBUFW(buf,51)=hd->hp;
	WBUFW(buf,53)=hd->max_hp;
	WBUFW(buf,55)=hd->sp;
	WBUFW(buf,57)=hd->max_sp;
	WBUFL(buf,59)=hd->exp;
	WBUFL(buf,63)=hd->exp_next;
	WBUFW(buf,67)=hd->skillpts;
	WBUFW(buf,69)=0x21;
	clif_send(buf,/*packet_len_table[0x22e]*/71,&sd->bl,SELF); 
	return 0;
}

// like skillinfoblock, just for homunculi.
int clif_homunskillinfoblock(struct map_session_data *sd)
{
	int fd;
	int i,c,len=4,id/*, inf2*/;

	nullpo_retr(0, sd);
	nullpo_retr(0, sd->hd);

	fd=sd->fd;
	WFIFOHEAD(fd, 4 * 37 + 4);
	WFIFOW(fd,0)=0x235;
	for ( i = c = 0; i < 4; i++){
		if( (id=sd->hd->hskill[i].id)!=0 ){
			WFIFOW(fd,len  ) = id;
			WFIFOW(fd,len+2) = skill_get_inf(id-7300);		// H. skills mapped to 700 and above
			WFIFOW(fd,len+4) = 0;
			WFIFOW(fd,len+6) = sd->hd->hskill[i].level;
			WFIFOW(fd,len+8) = skill_get_sp(id,sd->hd->hskill[i].level);
			WFIFOW(fd,len+10)= skill_get_range2(&sd->bl, id,sd->hd->hskill[i].level);
			strncpy(WFIFOP(fd,len+12), /*merc_skill_get_name(id)*/ "", NAME_LENGTH); // can somebody tell me what exactly that function was good for anyway
		/*	inf2 = skill_get_inf2(id);
			if(((!(inf2&INF2_QUEST_SKILL) || battle_config.quest_skill_learn) &&
				!(inf2&(INF2_WEDDING_SKILL|INF2_SPIRIT_SKILL))) ||
				(battle_config.gm_allskill > 0 && pc_isGM(sd) >= battle_config.gm_allskill) )
				//WFIFOB(fd,len+36)= (sd->status.skill[i].lv < skill_get_max(id) && sd->status.skill[i].flag ==0 )? 1:0;
				WFIFOB(fd,len+36)= (sd->status.skill[i].lv < skill_tree_get_max(id, sd->status.class_) && sd->status.skill[i].flag ==0 )? 1:0;
			else */
				WFIFOB(fd,len+36) = 1;//0;
			len+=37;
			c++;
		}
	}
	WFIFOW(fd,2)=len;
	WFIFOSET(fd,len);

	return 0;
}

// Request a Homunculus name change
void clif_parse_ChangeHomunculusName(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);
	nullpo_retv(sd);
	nullpo_retv(sd->hd);
	memcpy(sd->hd->name,RFIFOP(fd,2),24);
	clif_homuninfo(sd);
	clif_charnameack(sd->fd,&sd->hd->bl);
}

// Somebody who is less lazy than me rename this to ReturnToMaster or something
void clif_parse_QueryHomunPos(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);
	nullpo_retv(sd);
	nullpo_retv(sd->hd);
	unit_walktoxy(&sd->hd->bl, sd->bl.x,sd->bl.y-1, 0); //move to master
	//clif_homunposack(sd->hd);
}

// Request a Homunculus move-to-position
void clif_parse_HMoveTo(int fd,struct map_session_data *sd) {
	int x,y,cmd;

	nullpo_retv(sd);
	nullpo_retv(sd->hd);

	cmd = RFIFOW(fd,0);
	x = RFIFOB(fd,packet_db[sd->packet_ver][cmd].pos[0]) * 4 +
		(RFIFOB(fd,packet_db[sd->packet_ver][cmd].pos[0] + 1) >> 6);
	y = ((RFIFOB(fd,packet_db[sd->packet_ver][cmd].pos[0]+1) & 0x3f) << 4) +
		(RFIFOB(fd,packet_db[sd->packet_ver][cmd].pos[0] + 2) >> 4);

	unit_walktoxy(&sd->hd->bl,x,y,0);
}

// Request the Homunculus attacking a bl
void clif_parse_HAttack(int fd,struct map_session_data *sd) {
	nullpo_retv(sd);
	nullpo_retv(sd->hd);

	if(sd->hd->bl.id != RFIFOL(fd,2)) return;
	
	printf("unit_attack returned: %d\n",unit_attack(&sd->hd->bl,RFIFOL(fd,6),0));
}
/*==========================================
 *
 *------------------------------------------
 */
int clif_servertick(struct map_session_data *sd)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len_table[0x7f]);
	WFIFOW(fd,0)=0x7f;
	WFIFOL(fd,2)=sd->server_tick;
	WFIFOSET(fd,packet_len_table[0x7f]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_walkok(struct map_session_data *sd)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len_table[0x87]);
	WFIFOW(fd,0)=0x87;
	WFIFOL(fd,2)=gettick();
	WFIFOPOS2(fd,6,sd->bl.x,sd->bl.y,sd->ud.to_x,sd->ud.to_y);
	WFIFOB(fd,11)=0x88;
	WFIFOSET(fd,packet_len_table[0x87]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_movepc(struct map_session_data *sd) {
	unsigned char buf[256];

	nullpo_retr(0, sd);

	if (map[sd->bl.m].flag.snow
		|| map[sd->bl.m].flag.clouds
		|| map[sd->bl.m].flag.fog
		|| map[sd->bl.m].flag.fireworks
		|| map[sd->bl.m].flag.sakura
		|| map[sd->bl.m].flag.leaves
		|| map[sd->bl.m].flag.rain
		|| map[sd->bl.m].flag.clouds2
	) {
		memset(buf,0,packet_len_table[0x7b]);
		WBUFW(buf,0)=0x7b;
		WBUFL(buf,2)=-10;
		WBUFW(buf,6)=sd->speed;
		WBUFW(buf,8)=0;
		WBUFW(buf,10)=0;
		WBUFW(buf,12)=OPTION_INVISIBLE;
		WBUFW(buf,14)=100;
		WBUFL(buf,22)=gettick();
		WBUFPOS2(buf,50,sd->bl.x,sd->bl.y,sd->ud.to_x,sd->ud.to_y);
		WBUFB(buf,56)=5;
		WBUFB(buf,57)=5;
		clif_send(buf, packet_len_table[0x7b], &sd->bl, SELF);
	}

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_move(struct block_list *bl) {
	struct view_data *vd;
	struct unit_data *ud;
	unsigned char buf[256];
	int len;

	nullpo_retr(0, bl);
	
	vd = status_get_viewdata(bl);
	if (!vd || vd->class_ == INVISIBLE_CLASS)
		return 0;
	
	ud = unit_bl2ud(bl);
	nullpo_retr(0, ud);
	
	len = clif_set007b(bl,vd,ud,buf);
	clif_send(buf,len,bl,AREA_WOS);
	if (disguised(bl))
		clif_setdisguise((TBL_PC*)bl, buf, len, 0);
		
	//Stupid client that needs this resent every time someone walks :X
	if(vd->cloth_color)
		clif_refreshlook(bl,bl->id,LOOK_CLOTHES_COLOR,vd->cloth_color,AREA_WOS);

	switch(bl->type)
	{
	case BL_PC:
		{
			TBL_PC *sd = ((TBL_PC*)bl);
//			clif_movepc(sd);
			if(sd->state.size==2) // tiny/big players [Valaris]
				clif_specialeffect(&sd->bl,423,0);
			else if(sd->state.size==1)
				clif_specialeffect(&sd->bl,421,0);
		}
		break;
	case BL_MOB:
		{
			TBL_MOB *md = ((TBL_MOB*)bl);
			if(md->special_state.size==2) // tiny/big mobs [Valaris]
				clif_specialeffect(&md->bl,423,0);
			else if(md->special_state.size==1)
				clif_specialeffect(&md->bl,421,0);
		}
		break;
	}
	return 0;
}


/*==========================================
 * Delays the map_quit of a player after they are disconnected. [Skotlex]
 *------------------------------------------
 */
static int clif_delayquit(int tid, unsigned int tick, int id, int data) {
	struct map_session_data *sd = NULL;

	if (chrif_isconnect())
	{	//Remove player from map server
		if ((sd = map_id2sd(id)) != NULL && sd->fd == 0) //Should be a disconnected player.
			map_quit(sd);
	} else //Save later.
		add_timer(tick + 10000, clif_delayquit, id, 0);
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_quitsave(int fd,struct map_session_data *sd)
{
	if (chrif_isconnect() && (sd->state.waitingdisconnect || //Was already waiting to be disconnected.
		!battle_config.prevent_logout || DIFF_TICK(gettick(), sd->canlog_tick) > battle_config.prevent_logout))
		map_quit(sd);
	else if (sd->fd)
	{	//Disassociate session from player (session is deleted after this function was called)
		//And set a timer to delete this player later.
		session[fd]->session_data = NULL;
		sd->fd = 0;
		add_timer(gettick() + 10000, clif_delayquit, sd->bl.id, 0);
	}
}

/*==========================================
 *
 *------------------------------------------
 */
static int clif_waitclose(int tid, unsigned int tick, int id, int data) {
	if (session[id] && session[id]->func_parse == clif_parse) //Avoid disconnecting non-players, as pointed out by End of Exam [Skotlex]
		session[id]->eof = 1;

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_setwaitclose(int fd) {
	struct map_session_data *sd;

	// if player is not already in the game (double connection probably)
	if ((sd = (struct map_session_data*)session[fd]->session_data) == NULL) {
		// limited timer, just to send information.
		add_timer(gettick() + 1000, clif_waitclose, fd, 0);
	} else
		add_timer(gettick() + 5000, clif_waitclose, fd, 0);
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_changemap(struct map_session_data *sd, short map, int x, int y) {
	int fd;
	
	nullpo_retr(0, sd);

	fd = sd->fd;
	
	WFIFOHEAD(fd, packet_len_table[0x91]);
	WFIFOW(fd,0) = 0x91;
	memcpy(WFIFOP(fd,2), mapindex_id2name(map), MAP_NAME_LENGTH);
	WFIFOW(fd,18) = x;
	WFIFOW(fd,20) = y;
	WFIFOSET(fd, packet_len_table[0x91]);

	if(pc_isdead(sd)) // If player is dead, and is spawned (such as @refresh) send death packet. [Valaris]
		clif_clearchar_area(&sd->bl,1);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_changemapserver(struct map_session_data *sd, char *mapname, int x, int y, int ip, int port) {
	int fd;

	nullpo_retr(0, sd);

	fd = sd->fd;
	WFIFOHEAD(fd, packet_len_table[0x92]);
	WFIFOW(fd,0) = 0x92;
	//Better not trust the null-terminator is there. [Skotlex]
	memcpy(WFIFOP(fd,2), mapname, MAP_NAME_LENGTH);
	WFIFOB(fd,17) = 0;	//Null terminator for mapname
	WFIFOW(fd,18) = x;
	WFIFOW(fd,20) = y;
	WFIFOL(fd,22) = ip;
	WFIFOW(fd,26) = port;
	WFIFOSET(fd, packet_len_table[0x92]);
	
	return 0;
}

int clif_blown(struct block_list *bl) {
//Previous Aegis versions simply used clif_fixpos, but it seems clif_slide works better on current clients.
//	return clif_fixpos(bl);
	return clif_slide(bl, bl->x, bl->y);
	
}
/*==========================================
 *
 *------------------------------------------
 */
int clif_fixpos(struct block_list *bl) {
	unsigned char buf[16];

	nullpo_retr(0, bl);

	WBUFW(buf,0)=0x88;
	WBUFL(buf,2)=bl->id;
	WBUFW(buf,6)=bl->x;
	WBUFW(buf,8)=bl->y;
	clif_send(buf, packet_len_table[0x88], bl, AREA);
	if (disguised(bl)) {
		WBUFL(buf,2)=-bl->id;
		clif_send(buf, packet_len_table[0x88], bl, SELF);
	}
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_npcbuysell(struct map_session_data* sd, int id) {
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len_table[0xc4]);
	WFIFOW(fd,0)=0xc4;
	WFIFOL(fd,2)=id;
	WFIFOSET(fd,packet_len_table[0xc4]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_buylist(struct map_session_data *sd, struct npc_data *nd) {
	struct item_data *id;
	int fd,i,val;

	nullpo_retr(0, sd);
	nullpo_retr(0, nd);

	fd=sd->fd;
 	WFIFOHEAD(fd, 200 * 11 + 4);
	WFIFOW(fd,0)=0xc6;
	for(i=0;nd->u.shop_item[i].nameid > 0;i++){
		id = itemdb_search(nd->u.shop_item[i].nameid);
		val=nd->u.shop_item[i].value;
		WFIFOL(fd,4+i*11)=val;
		if (!id->flag.value_notdc)
			val=pc_modifybuyvalue(sd,val);
		WFIFOL(fd,8+i*11)=val;
		WFIFOB(fd,12+i*11)=itemtype(id->type);
		if (id->view_id > 0)
			WFIFOW(fd,13+i*11)=id->view_id;
		else
			WFIFOW(fd,13+i*11)=nd->u.shop_item[i].nameid;
	}
	WFIFOW(fd,2)=i*11+4;
	WFIFOSET(fd,WFIFOW(fd,2));

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_selllist(struct map_session_data *sd) {
	int fd,i,c=0,val;

	nullpo_retr(0, sd);

	fd=sd->fd;
        WFIFOHEAD(fd, MAX_INVENTORY * 10 + 4);
	WFIFOW(fd,0)=0xc7;
	for(i=0;i<MAX_INVENTORY;i++) {
		if(sd->status.inventory[i].nameid > 0 && sd->inventory_data[i]) {
			if (!itemdb_cansell(sd->status.inventory[i].nameid, pc_isGM(sd)))
				continue;

			val=sd->inventory_data[i]->value_sell;
			if (val < 0)
				continue;
			WFIFOW(fd,4+c*10)=i+2;
			WFIFOL(fd,6+c*10)=val;
			if (!sd->inventory_data[i]->flag.value_notoc)
				val=pc_modifysellvalue(sd,val);
			WFIFOL(fd,10+c*10)=val;
			c++;
		}
	}
	WFIFOW(fd,2)=c*10+4;
	WFIFOSET(fd,WFIFOW(fd,2));

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_scriptmes(struct map_session_data *sd, int npcid, char *mes) {
	int fd;
	int slen = strlen(mes) + 9;
	WFIFOHEAD(fd, slen);

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOW(fd,0)=0xb4;
	WFIFOW(fd,2)=slen;
	WFIFOL(fd,4)=npcid;
	strcpy((char*)WFIFOP(fd,8),mes);
	WFIFOSET(fd,WFIFOW(fd,2));

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_scriptnext(struct map_session_data *sd,int npcid) {
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len_table[0xb5]);
	WFIFOW(fd,0)=0xb5;
	WFIFOL(fd,2)=npcid;
	WFIFOSET(fd,packet_len_table[0xb5]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_scriptclose(struct map_session_data *sd, int npcid) {
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len_table[0xb6]);
	WFIFOW(fd,0)=0xb6;
	WFIFOL(fd,2)=npcid;
	WFIFOSET(fd,packet_len_table[0xb6]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_sendfakenpc(struct map_session_data *sd, int npcid) {
	int fd = sd->fd;
	sd->npc_id = npcid;
	sd->state.using_fake_npc = 1;
	memset(WFIFOP(fd,0), 0, packet_len_table[0x78]);
	WFIFOW(fd,0)=0x78;
	WFIFOL(fd,2)=npcid;
	WFIFOW(fd,14)=111;
	WFIFOPOS(fd,46,sd->bl.x,sd->bl.y);
	WFIFOB(fd,49)=5;
	WFIFOB(fd,50)=5;
	WFIFOSET(fd, packet_len_table[0x78]);
	return;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_scriptmenu(struct map_session_data *sd, int npcid, char *mes) {
	int fd;
	int slen = strlen(mes) + 8;
	struct block_list *bl = NULL;
	WFIFOHEAD(fd, slen);

	nullpo_retr(0, sd);

	if (!sd->state.using_fake_npc && (npcid == fake_npc_id || ((bl = map_id2bl(npcid)) && (bl->m!=sd->bl.m ||
	   bl->x<sd->bl.x-AREA_SIZE-1 || bl->x>sd->bl.x+AREA_SIZE+1 ||
	   bl->y<sd->bl.y-AREA_SIZE-1 || bl->y>sd->bl.y+AREA_SIZE+1))))
	   clif_sendfakenpc(sd, npcid);

	fd=sd->fd;
	WFIFOW(fd,0)=0xb7;
	WFIFOW(fd,2)=slen;
	WFIFOL(fd,4)=npcid;
	strcpy((char*)WFIFOP(fd,8),mes);
	WFIFOSET(fd,WFIFOW(fd,2));
	
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_scriptinput(struct map_session_data *sd, int npcid) {
	int fd;
	struct block_list *bl = NULL;

	nullpo_retr(0, sd);

	if (!sd->state.using_fake_npc && (npcid == fake_npc_id || ((bl = map_id2bl(npcid)) && (bl->m!=sd->bl.m ||
	   bl->x<sd->bl.x-AREA_SIZE-1 || bl->x>sd->bl.x+AREA_SIZE+1 ||
	   bl->y<sd->bl.y-AREA_SIZE-1 || bl->y>sd->bl.y+AREA_SIZE+1))))
	   clif_sendfakenpc(sd, npcid);
	
	fd=sd->fd;
	WFIFOHEAD(fd, packet_len_table[0x142]);
	WFIFOW(fd,0)=0x142;
	WFIFOL(fd,2)=npcid;
	WFIFOSET(fd,packet_len_table[0x142]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_scriptinputstr(struct map_session_data *sd, int npcid) {
	int fd;
	struct block_list *bl = NULL;

	nullpo_retr(0, sd);

	if (!sd->state.using_fake_npc && (npcid == fake_npc_id || ((bl = map_id2bl(npcid)) && (bl->m!=sd->bl.m ||
	   bl->x<sd->bl.x-AREA_SIZE-1 || bl->x>sd->bl.x+AREA_SIZE+1 ||
	   bl->y<sd->bl.y-AREA_SIZE-1 || bl->y>sd->bl.y+AREA_SIZE+1))))
	   clif_sendfakenpc(sd, npcid);

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len_table[0x1d4]);
	WFIFOW(fd,0)=0x1d4;
	WFIFOL(fd,2)=npcid;
	WFIFOSET(fd,packet_len_table[0x1d4]);
	
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_viewpoint(struct map_session_data *sd, int npc_id, int type, int x, int y, int id, int color) {
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len_table[0x144]);
	WFIFOW(fd,0)=0x144;
	WFIFOL(fd,2)=npc_id;
	WFIFOL(fd,6)=type;
	WFIFOL(fd,10)=x;
	WFIFOL(fd,14)=y;
	WFIFOB(fd,18)=id;
	WFIFOL(fd,19)=color;
	WFIFOSET(fd,packet_len_table[0x144]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_cutin(struct map_session_data *sd, char *image, int type) {
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len_table[0x1b3]);
	WFIFOW(fd,0)=0x1b3;
	strncpy((char*)WFIFOP(fd,2),image,64);
	WFIFOB(fd,66)=type;
	WFIFOSET(fd,packet_len_table[0x1b3]);

	return 0;
}

/*==========================================
 * Fills in card data from the given item and into the buffer. [Skotlex]
 *------------------------------------------
 */
static void clif_addcards(unsigned char* buf, struct item* item)
{
	int i=0,j;
	if (item == NULL) { //Blank data
		WBUFW(buf,0)=0;
		WBUFW(buf,2)=0;
		WBUFW(buf,4)=0;
		WBUFW(buf,6)=0;
		return;
	}
	if(item->card[0]==(short)0xff00) { //pet eggs
		WBUFW(buf,0)=0;
		WBUFW(buf,2)=0;
		WBUFW(buf,4)=0;
		WBUFW(buf,6)=item->card[3]; //Pet renamed flag.
		return;
	}
	if(item->card[0]==0x00ff || item->card[0]==0x00fe) { //Forged/created items
		WBUFW(buf,0)=item->card[0];
		WBUFW(buf,2)=item->card[1];
		WBUFW(buf,4)=item->card[2];
		WBUFW(buf,6)=item->card[3];
		return;
	}
	//Client only receives four cards.. so randomly send them a set of cards. [Skotlex]
	if (MAX_SLOTS > 4 && (j = itemdb_slot(item->nameid)) > 4)
		i = rand()%(j-3); //eg: 6 slots, possible i values: 0->3, 1->4, 2->5 => i = rand()%3;

	//Normal items.
	if (item->card[i] > 0 && (j=itemdb_viewid(item->card[i])) > 0)
		WBUFW(buf,0)=j;
	else
		WBUFW(buf,0)= item->card[i];

	if (item->card[++i] > 0 && (j=itemdb_viewid(item->card[i])) > 0)
		WBUFW(buf,2)=j;
	else
		WBUFW(buf,2)=item->card[i];

	if (item->card[++i] > 0 && (j=itemdb_viewid(item->card[i])) > 0)
		WBUFW(buf,4)=j;
	else
		WBUFW(buf,4)=item->card[i];

	if (item->card[++i] > 0 && (j=itemdb_viewid(item->card[i])) > 0)
		WBUFW(buf,6)=j;
	else
		WBUFW(buf,6)=item->card[i];
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_additem(struct map_session_data *sd, int n, int amount, int fail) {
	int fd;
	unsigned char *buf;

	nullpo_retr(0, sd);

	fd = sd->fd;
	if (!session_isActive(fd))  //Sasuke-
		return 0;

	WFIFOHEAD(fd,packet_len_table[0xa0]);
	buf = WFIFOP(fd,0);
	if(fail) {
		WBUFW(buf,0)=0xa0;
		WBUFW(buf,2)=n+2;
		WBUFW(buf,4)=amount;
		WBUFW(buf,6)=0;
		WBUFB(buf,8)=0;
		WBUFB(buf,9)=0;
		WBUFB(buf,10)=0;
		WBUFW(buf,11)=0;
		WBUFW(buf,13)=0;
		WBUFW(buf,15)=0;
		WBUFW(buf,17)=0;
		WBUFW(buf,19)=0;
		WBUFB(buf,21)=0;
		WBUFB(buf,22)=fail;
	} else {
		if (n<0 || n>=MAX_INVENTORY || sd->status.inventory[n].nameid <=0 || sd->inventory_data[n] == NULL)
			return 1;

		WBUFW(buf,0)=0xa0;
		WBUFW(buf,2)=n+2;
		WBUFW(buf,4)=amount;
		if (sd->inventory_data[n]->view_id > 0)
			WBUFW(buf,6)=sd->inventory_data[n]->view_id;
		else
			WBUFW(buf,6)=sd->status.inventory[n].nameid;
		WBUFB(buf,8)=sd->status.inventory[n].identify;
		WBUFB(buf,9)=sd->status.inventory[n].attribute;
		WBUFB(buf,10)=sd->status.inventory[n].refine;
		clif_addcards(WBUFP(buf,11), &sd->status.inventory[n]);
		WBUFW(buf,19)=pc_equippoint(sd,n);
		WBUFB(buf,21)=itemtype(sd->inventory_data[n]->type);
		WBUFB(buf,22)=fail;
	}

	WFIFOSET(fd,packet_len_table[0xa0]);
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_delitem(struct map_session_data *sd,int n,int amount)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
        WFIFOHEAD(fd, packet_len_table[0xaf]);
	WFIFOW(fd,0)=0xaf;
	WFIFOW(fd,2)=n+2;
	WFIFOW(fd,4)=amount;

	WFIFOSET(fd,packet_len_table[0xaf]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_itemlist(struct map_session_data *sd)
{
	int i,n,fd,arrow=-1;
	unsigned char *buf;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd, MAX_INVENTORY * 10 + 4);
	buf = WFIFOP(fd,0);
#if PACKETVER < 5
	WBUFW(buf,0)=0xa3;
	for(i=0,n=0;i<MAX_INVENTORY;i++){
		if (sd->status.inventory[i].nameid <=0 || sd->inventory_data[i] == NULL || itemdb_isequip2(sd->inventory_data[i]))
			continue;
		WBUFW(buf,n*10+4)=i+2;
		if (sd->inventory_data[i]->view_id > 0)
			WBUFW(buf,n*10+6)=sd->inventory_data[i]->view_id;
		else
			WBUFW(buf,n*10+6)=sd->status.inventory[i].nameid;
		WBUFB(buf,n*10+8)=itemtype(sd->inventory_data[i]->type);
		WBUFB(buf,n*10+9)=sd->status.inventory[i].identify;
		WBUFW(buf,n*10+10)=sd->status.inventory[i].amount;
		if (sd->inventory_data[i]->equip == 0x8000) {
			WBUFW(buf,n*10+12)=0x8000;
			if (sd->status.inventory[i].equip)
				arrow=i;	// ついでに矢装備チェック
		} else
			WBUFW(buf,n*10+12)=0;
		n++;
	}
	if (n) {
		WBUFW(buf,2)=4+n*10;
		WFIFOSET(fd,WFIFOW(fd,2));
	}
#else
	WBUFW(buf,0)=0x1ee;
	for(i=0,n=0;i<MAX_INVENTORY;i++){
		if(sd->status.inventory[i].nameid <=0 || sd->inventory_data[i] == NULL || itemdb_isequip2(sd->inventory_data[i]))
			continue;
		WBUFW(buf,n*18+4)=i+2;
		if(sd->inventory_data[i]->view_id > 0)
			WBUFW(buf,n*18+6)=sd->inventory_data[i]->view_id;
		else
			WBUFW(buf,n*18+6)=sd->status.inventory[i].nameid;
		WBUFB(buf,n*18+8)=itemtype(sd->inventory_data[i]->type);
		WBUFB(buf,n*18+9)=sd->status.inventory[i].identify;
		WBUFW(buf,n*18+10)=sd->status.inventory[i].amount;
		if (sd->inventory_data[i]->equip == 0x8000) {
			WBUFW(buf,n*18+12)=0x8000;
			if(sd->status.inventory[i].equip)
				arrow=i;	// ついでに矢装備チェック
		} else
			WBUFW(buf,n*18+12)=0;
		clif_addcards(WBUFP(buf, n*18+14), &sd->status.inventory[i]);
		n++;
	}
	if (n) {
		WBUFW(buf,2)=4+n*18;
		WFIFOSET(fd,WFIFOW(fd,2));
	}
#endif
	if(arrow >= 0)
		clif_arrowequip(sd,arrow);
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_equiplist(struct map_session_data *sd)
{
	int i,n,fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	if (!session_isActive(fd))
		return 0;
        WFIFOHEAD(fd, 4 + MAX_INVENTORY * 20);
	WFIFOW(fd,0)=0xa4;
	for(i=0,n=0;i<MAX_INVENTORY;i++){
		if(sd->status.inventory[i].nameid<=0 || sd->inventory_data[i] == NULL || !itemdb_isequip2(sd->inventory_data[i]))
			continue;
		WFIFOW(fd,n*20+4)=i+2;
		if(sd->inventory_data[i]->view_id > 0)
			WFIFOW(fd,n*20+6)=sd->inventory_data[i]->view_id;
		else
			WFIFOW(fd,n*20+6)=sd->status.inventory[i].nameid;
		WFIFOB(fd,n*20+8)=itemtype(sd->inventory_data[i]->type);
		WFIFOB(fd,n*20+9)=sd->status.inventory[i].identify;
		WFIFOW(fd,n*20+10)=pc_equippoint(sd,i);
		WFIFOW(fd,n*20+12)=sd->status.inventory[i].equip;
		WFIFOB(fd,n*20+14)=sd->status.inventory[i].attribute;
		WFIFOB(fd,n*20+15)=sd->status.inventory[i].refine;
		clif_addcards(WFIFOP(fd, n*20+16), &sd->status.inventory[i]);
		n++;
	}
	if(n){
		WFIFOW(fd,2)=4+n*20;
		WFIFOSET(fd,WFIFOW(fd,2));
	}
	return 0;
}

/*==========================================
 * カプラさんに預けてある消耗品&収集品リスト
 *------------------------------------------
 */
int clif_storageitemlist(struct map_session_data *sd,struct storage *stor)
{
	struct item_data *id;
	int i,n,fd;
	unsigned char *buf;

	nullpo_retr(0, sd);
	nullpo_retr(0, stor);

	fd=sd->fd;
	WFIFOHEAD(fd,MAX_STORAGE * 18 + 4);
	buf = WFIFOP(fd,0);
#if PACKETVER < 5
	WBUFW(buf,0)=0xa5;
	for(i=0,n=0;i<MAX_STORAGE;i++){
		if(stor->storage_[i].nameid<=0)
			continue;
		nullpo_retr(0, id = itemdb_search(stor->storage_[i].nameid));
		if(itemdb_isequip2(id))
			continue;

		WBUFW(buf,n*10+4)=i+1;
		if(id->view_id > 0)
			WBUFW(buf,n*10+6)=id->view_id;
		else
			WBUFW(buf,n*10+6)=stor->storage_[i].nameid;
		WBUFB(buf,n*10+8)=itemtype(id->type);
		WBUFB(buf,n*10+9)=stor->storage_[i].identify;
		WBUFW(buf,n*10+10)=stor->storage_[i].amount;
		WBUFW(buf,n*10+12)=0;
		n++;
	}
	if(n){
		WBUFW(buf,2)=4+n*10;
		WFIFOSET(fd,WFIFOW(fd,2));
	}
#else
	WBUFW(buf,0)=0x1f0;
	for(i=0,n=0;i<MAX_STORAGE;i++){
		if(stor->storage_[i].nameid<=0)
			continue;
		nullpo_retr(0, id = itemdb_search(stor->storage_[i].nameid));
		if(itemdb_isequip2(id))
			continue;

		WBUFW(buf,n*18+4)=i+1;
		if(id->view_id > 0)
			WBUFW(buf,n*18+6)=id->view_id;
		else
			WBUFW(buf,n*18+6)=stor->storage_[i].nameid;
		WBUFB(buf,n*18+8)=itemtype(id->type);
		WBUFB(buf,n*18+9)=stor->storage_[i].identify;
		WBUFW(buf,n*18+10)=stor->storage_[i].amount;
		WBUFW(buf,n*18+12)=0;
		clif_addcards(WBUFP(buf,n*18+14), &stor->storage_[i]);
		n++;
	}
	if(n){
		WBUFW(buf,2)=4+n*18;
		WFIFOSET(fd,WFIFOW(fd,2));
	}
#endif
	return 0;
}

/*==========================================
 * カプラさんに預けてある装備リスト
 *------------------------------------------
 */
int clif_storageequiplist(struct map_session_data *sd,struct storage *stor)
{
	struct item_data *id;
	int i,n,fd;
	unsigned char *buf;

	nullpo_retr(0, sd);
	nullpo_retr(0, stor);

	fd=sd->fd;
	WFIFOHEAD(fd,MAX_STORAGE * 20 + 4);
	buf = WFIFOP(fd,0);
	WBUFW(buf,0)=0xa6;
	for(i=0,n=0;i<MAX_STORAGE;i++){
		if(stor->storage_[i].nameid<=0)
			continue;
		nullpo_retr(0, id = itemdb_search(stor->storage_[i].nameid));
		if(!itemdb_isequip2(id))
			continue;
		WBUFW(buf,n*20+4)=i+1;
		if(id->view_id > 0)
			WBUFW(buf,n*20+6)=id->view_id;
		else
			WBUFW(buf,n*20+6)=stor->storage_[i].nameid;
		WBUFB(buf,n*20+8)=itemtype(id->type);
		WBUFB(buf,n*20+9)=stor->storage_[i].identify;
		WBUFW(buf,n*20+10)=id->equip;
		WBUFW(buf,n*20+12)=stor->storage_[i].equip;
		WBUFB(buf,n*20+14)=stor->storage_[i].attribute;
		WBUFB(buf,n*20+15)=stor->storage_[i].refine;
		clif_addcards(WBUFP(buf, n*20+16), &stor->storage_[i]);
		n++;
	}
	if(n){
		WBUFW(buf,2)=4+n*20;
		WFIFOSET(fd,WFIFOW(fd,2));
	}
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_guildstorageitemlist(struct map_session_data *sd,struct guild_storage *stor)
{
	struct item_data *id;
	int i,n,fd;
	unsigned char *buf;

	nullpo_retr(0, sd);
	nullpo_retr(0, stor);

	fd=sd->fd;
	WFIFOHEAD(fd, MAX_GUILD_STORAGE * 18 + 4);
	buf=WFIFOP(fd,0);

#if PACKETVER < 5
	WBUFW(buf,0)=0xa5;
	for(i=0,n=0;i<MAX_GUILD_STORAGE;i++){
		if(stor->storage_[i].nameid<=0)
			continue;
		nullpo_retr(0, id = itemdb_search(stor->storage_[i].nameid));
		if(itemdb_isequip2(id))
			continue;

		WBUFW(buf,n*10+4)=i+1;
		if(id->view_id > 0)
			WBUFW(buf,n*10+6)=id->view_id;
		else
			WBUFW(buf,n*10+6)=stor->storage_[i].nameid;
		WBUFB(buf,n*10+8)=itemtype(id->type);
		WBUFB(buf,n*10+9)=stor->storage_[i].identify;
		WBUFW(buf,n*10+10)=stor->storage_[i].amount;
		WBUFW(buf,n*10+12)=0;
		n++;
	}
	if(n){
		WBUFW(buf,2)=4+n*10;
		WFIFOSET(fd,WFIFOW(fd,2));
	}
#else
	WBUFW(buf,0)=0x1f0;
	for(i=0,n=0;i<MAX_GUILD_STORAGE;i++){
		if(stor->storage_[i].nameid<=0)
			continue;
		nullpo_retr(0, id = itemdb_search(stor->storage_[i].nameid));
		if(itemdb_isequip2(id))
			continue;

		WBUFW(buf,n*18+4)=i+1;
		if(id->view_id > 0)
			WBUFW(buf,n*18+6)=id->view_id;
		else
			WBUFW(buf,n*18+6)=stor->storage_[i].nameid;
		WBUFB(buf,n*18+8)=itemtype(id->type);
		WBUFB(buf,n*18+9)=stor->storage_[i].identify;
		WBUFW(buf,n*18+10)=stor->storage_[i].amount;
		WBUFW(buf,n*18+12)=0;
		clif_addcards(WBUFP(buf,n*18+14), &stor->storage_[i]);
		n++;
	}
	if(n){
		WBUFW(buf,2)=4+n*18;
		WFIFOSET(fd,WFIFOW(fd,2));
	}
#endif
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_guildstorageequiplist(struct map_session_data *sd,struct guild_storage *stor)
{
	struct item_data *id;
	int i,n,fd;
	unsigned char *buf;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd, MAX_GUILD_STORAGE * 20 + 4);
	buf=WFIFOP(fd,0);

	WBUFW(buf,0)=0xa6;
	for(i=0,n=0;i<MAX_GUILD_STORAGE;i++){
		if(stor->storage_[i].nameid<=0)
			continue;
		nullpo_retr(0, id = itemdb_search(stor->storage_[i].nameid));
		if(!itemdb_isequip2(id))
			continue;
		WBUFW(buf,n*20+4)=i+1;
		if(id->view_id > 0)
			WBUFW(buf,n*20+6)=id->view_id;
		else
			WBUFW(buf,n*20+6)=stor->storage_[i].nameid;
		WBUFB(buf,n*20+8)=itemtype(id->type);
		WBUFB(buf,n*20+9)=stor->storage_[i].identify;
		WBUFW(buf,n*20+10)=id->equip;
		WBUFW(buf,n*20+12)=stor->storage_[i].equip;
		WBUFB(buf,n*20+14)=stor->storage_[i].attribute;
		WBUFB(buf,n*20+15)=stor->storage_[i].refine;
		clif_addcards(WBUFP(buf, n*20+16), &stor->storage_[i]);
		n++;
	}
	if(n){
		WBUFW(buf,2)=4+n*20;
		WFIFOSET(fd,WFIFOW(fd,2));
	}
	return 0;
}

// Guild XY locators [Valaris]
int clif_guild_xy(struct map_session_data *sd)
{
	unsigned char buf[10];

	nullpo_retr(0, sd);

	WBUFW(buf,0)=0x1eb;
	WBUFL(buf,2)=sd->status.account_id;
	WBUFW(buf,6)=sd->bl.x;
	WBUFW(buf,8)=sd->bl.y;
	clif_send(buf,packet_len_table[0x1eb],&sd->bl,GUILD_SAMEMAP_WOS);

	return 0;
}

/*==========================================
 * Sends x/y dot to a single fd. [Skotlex]
 *------------------------------------------
 */

int clif_guild_xy_single(int fd, struct map_session_data *sd)
{
	WFIFOHEAD(fd,packet_len_table[0x1eb]);
	WFIFOW(fd,0)=0x1eb;
	WFIFOL(fd,2)=sd->status.account_id;
	WFIFOW(fd,6)=sd->bl.x;
	WFIFOW(fd,8)=sd->bl.y;
	WFIFOSET(fd,packet_len_table[0x1eb]);
	return 0;
}

// Guild XY locators [Valaris]
int clif_guild_xy_remove(struct map_session_data *sd)
{
	unsigned char buf[10];

	nullpo_retr(0, sd);

	WBUFW(buf,0)=0x1eb;
	WBUFL(buf,2)=sd->status.account_id;
	WBUFW(buf,6)=-1;
	WBUFW(buf,8)=-1;
	clif_send(buf,packet_len_table[0x1eb],&sd->bl,GUILD_SAMEMAP_WOS);

	return 0;
}

/*==========================================
 * ステータスを送りつける
 * 表示専用数字はこの中で計算して送る
 *------------------------------------------
 */
int clif_updatestatus(struct map_session_data *sd,int type)
{
	int fd,len=8;

	nullpo_retr(0, sd);

	fd=sd->fd;

	if ( !session_isActive(fd) ) // Invalid pointer fix, by sasuke [Kevin]
		return 0;
 
	WFIFOHEAD(fd, 14);
	WFIFOW(fd,0)=0xb0;
	WFIFOW(fd,2)=type;
	switch(type){
		// 00b0
	case SP_WEIGHT:
		pc_checkweighticon(sd);
		WFIFOW(fd,0)=0xb0;
		WFIFOW(fd,2)=type;	//Added this packet back, Temp fix to the slow motion [Lupus]
		WFIFOL(fd,4)=sd->weight;
		break;
	case SP_MAXWEIGHT:
		WFIFOL(fd,4)=sd->max_weight;
		break;
	case SP_SPEED:
		WFIFOL(fd,4)=sd->speed;
		break;
	case SP_BASELEVEL:
		WFIFOL(fd,4)=sd->status.base_level;
		break;
	case SP_JOBLEVEL:
		WFIFOL(fd,4)=sd->status.job_level;
		break;
	case SP_MANNER:
		WFIFOL(fd,4)=sd->status.manner;
		clif_changestatus(&sd->bl,SP_MANNER,sd->status.manner);
		break;
	case SP_STATUSPOINT:
		WFIFOL(fd,4)=sd->status.status_point;
		break;
	case SP_SKILLPOINT:
		WFIFOL(fd,4)=sd->status.skill_point;
		break;
	case SP_HIT:
		WFIFOL(fd,4)=sd->hit;
		break;
	case SP_FLEE1:
		WFIFOL(fd,4)=sd->flee;
		break;
	case SP_FLEE2:
		WFIFOL(fd,4)=sd->flee2/10;
		break;
	case SP_MAXHP:
		WFIFOL(fd,4)=sd->status.max_hp;
		break;
	case SP_MAXSP:
		WFIFOL(fd,4)=sd->status.max_sp;
		break;
	case SP_HP:
		WFIFOL(fd,4)=sd->status.hp;
		if (sd->status.party_id)
			clif_party_hp(sd);
		if (battle_config.disp_hpmeter)
			clif_hpmeter(sd);
		break;
	case SP_SP:
		WFIFOL(fd,4)=sd->status.sp;
		break;
	case SP_ASPD:
		WFIFOL(fd,4)=sd->aspd;
		break;
	case SP_ATK1:
		WFIFOL(fd,4)=sd->base_atk+sd->right_weapon.watk+sd->left_weapon.watk;
		break;
	case SP_DEF1:
		WFIFOL(fd,4)=sd->def;
		break;
	case SP_MDEF1:
		WFIFOL(fd,4)=sd->mdef;
		break;
	case SP_ATK2:
		WFIFOL(fd,4)=sd->right_weapon.watk2 + sd->left_weapon.watk2;
		break;
	case SP_DEF2:
		WFIFOL(fd,4)=sd->def2;
		break;
	case SP_MDEF2:
		WFIFOL(fd,4)=sd->mdef2;
		break;
	case SP_CRITICAL:
		WFIFOL(fd,4)=sd->critical/10;
		break;
	case SP_MATK1:
		WFIFOL(fd,4)=sd->matk1;
		break;
	case SP_MATK2:
		WFIFOL(fd,4)=sd->matk2;
		break;


	case SP_ZENY:
		WFIFOW(fd,0)=0xb1;
		if(sd->status.zeny < 0)
			sd->status.zeny = 0;
		WFIFOL(fd,4)=sd->status.zeny;
		break;
	case SP_BASEEXP:
		WFIFOW(fd,0)=0xb1;
		WFIFOL(fd,4)=sd->status.base_exp;
		break;
	case SP_JOBEXP:
		WFIFOW(fd,0)=0xb1;
		WFIFOL(fd,4)=sd->status.job_exp;
		break;
	case SP_NEXTBASEEXP:
		WFIFOW(fd,0)=0xb1;
		WFIFOL(fd,4)=pc_nextbaseexp(sd);
		break;
	case SP_NEXTJOBEXP:
		WFIFOW(fd,0)=0xb1;
		WFIFOL(fd,4)=pc_nextjobexp(sd);
		break;

		// 00be 終了
	case SP_USTR:
	case SP_UAGI:
	case SP_UVIT:
	case SP_UINT:
	case SP_UDEX:
	case SP_ULUK:
		WFIFOW(fd,0)=0xbe;
		WFIFOB(fd,4)=pc_need_status_point(sd,type-SP_USTR+SP_STR);
		len=5;
		break;

		// 013a 終了
	case SP_ATTACKRANGE:
		WFIFOW(fd,0)=0x13a;
		WFIFOW(fd,2)=sd->attackrange;
		len=4;
		break;

		// 0141 終了
	case SP_STR:
		WFIFOW(fd,0)=0x141;
		WFIFOL(fd,2)=type;
		WFIFOL(fd,6)=sd->status.str;
		WFIFOL(fd,10)=sd->paramb[0] + sd->parame[0];
		len=14;
		break;
	case SP_AGI:
		WFIFOW(fd,0)=0x141;
		WFIFOL(fd,2)=type;
		WFIFOL(fd,6)=sd->status.agi;
		WFIFOL(fd,10)=sd->paramb[1] + sd->parame[1];
		len=14;
		break;
	case SP_VIT:
		WFIFOW(fd,0)=0x141;
		WFIFOL(fd,2)=type;
		WFIFOL(fd,6)=sd->status.vit;
		WFIFOL(fd,10)=sd->paramb[2] + sd->parame[2];
		len=14;
		break;
	case SP_INT:
		WFIFOW(fd,0)=0x141;
		WFIFOL(fd,2)=type;
		WFIFOL(fd,6)=sd->status.int_;
		WFIFOL(fd,10)=sd->paramb[3] + sd->parame[3];
		len=14;
		break;
	case SP_DEX:
		WFIFOW(fd,0)=0x141;
		WFIFOL(fd,2)=type;
		WFIFOL(fd,6)=sd->status.dex;
		WFIFOL(fd,10)=sd->paramb[4] + sd->parame[4];
		len=14;
		break;
	case SP_LUK:
		WFIFOW(fd,0)=0x141;
		WFIFOL(fd,2)=type;
		WFIFOL(fd,6)=sd->status.luk;
		WFIFOL(fd,10)=sd->paramb[5] + sd->parame[5];
		len=14;
		break;

	case SP_CARTINFO:
		WFIFOW(fd,0)=0x121;
		WFIFOW(fd,2)=sd->cart_num;
		WFIFOW(fd,4)=sd->cart_max_num;
		WFIFOL(fd,6)=sd->cart_weight;
		WFIFOL(fd,10)=sd->cart_max_weight;
		len=14;
		break;

	default:
		if(battle_config.error_log)
			ShowError("clif_updatestatus : unrecognized type %d\n",type);
		return 1;
	}
	WFIFOSET(fd,len);

	return 0;
}
int clif_changestatus(struct block_list *bl,int type,int val)
{
	unsigned char buf[12];
	struct map_session_data *sd = NULL;

	nullpo_retr(0, bl);

	if(bl->type == BL_PC)
		sd = (struct map_session_data *)bl;

//printf("clif_changestatus id:%d type:%d val:%d\n",bl->id,type,val);
	if(sd){
		WBUFW(buf,0)=0x1ab;
		WBUFL(buf,2)=bl->id;
		WBUFW(buf,6)=type;
		switch(type){
		case SP_MANNER:
			WBUFL(buf,8)=val;
			break;
		default:
			if(battle_config.error_log)
				ShowError("clif_changestatus : unrecognized type %d.\n",type);
			return 1;
		}
		clif_send(buf,packet_len_table[0x1ab],bl,AREA_WOS);
	}
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_changelook(struct block_list *bl,int type,int val)
{
	unsigned char buf[32];
	struct map_session_data *sd = NULL;
	struct view_data *vd;
	vd = status_get_viewdata(bl);
	nullpo_retr(0, vd);
	
	if(bl->type == BL_PC)
		sd = (struct map_session_data *)bl;

	switch(type) {
		case LOOK_WEAPON:
			if (sd) clif_get_weapon_view(sd, &vd->weapon, &vd->shield);
			else vd->weapon = val;
		break;
		case LOOK_SHIELD:
			if (sd) clif_get_weapon_view(sd, &vd->weapon, &vd->shield);
			else vd->shield = val;
		break;
		case LOOK_BASE:
			vd->class_ = val;
			if (vd->class_ == JOB_WEDDING || vd->class_ == JOB_XMAS)
				vd->weapon = vd->shield = 0;
			if (vd->cloth_color && (
				(vd->class_ == JOB_WEDDING && battle_config.wedding_ignorepalette) ||
				(vd->class_ == JOB_XMAS && battle_config.xmas_ignorepalette)
			))
				clif_changelook(bl,LOOK_CLOTHES_COLOR,0);
		break;
		case LOOK_HAIR:
			vd->hair_style = val;
		break;
		case LOOK_HEAD_BOTTOM:
			vd->head_bottom = val;
		break;
		case LOOK_HEAD_TOP:
			vd->head_top = val;
		break;	
		case LOOK_HEAD_MID:
			vd->head_mid = val;
		break;
		case LOOK_HAIR_COLOR:
			vd->hair_color = val;
		break;
		case LOOK_CLOTHES_COLOR:
			if (
				(vd->class_ == JOB_WEDDING && battle_config.wedding_ignorepalette) ||
				(vd->class_ == JOB_XMAS && battle_config.xmas_ignorepalette)
			)
				val = 0;
			vd->cloth_color = val;
		break;
		case LOOK_SHOES:
#if PACKETVER > 3
			if (sd) {
				int n;
				if((n = sd->equip_index[2]) >= 0 && sd->inventory_data[n]) {
					if(sd->inventory_data[n]->view_id > 0)
						val = sd->inventory_data[n]->view_id;
					else
						val = sd->status.inventory[n].nameid;
					}
				val = 0;
			}
#endif
			//Shoes? No packet uses this....
		break;
	}
#if PACKETVER < 4
	WBUFW(buf,0)=0xc3;
	WBUFL(buf,2)=bl->id;
	WBUFB(buf,6)=type;
	WBUFB(buf,7)=val;
	clif_send(buf,packet_len_table[0xc3],bl,AREA);
#else
	if(type == LOOK_WEAPON || type == LOOK_SHIELD) {
		WBUFW(buf,0)=0x1d7;
		WBUFL(buf,2)=bl->id;
		WBUFB(buf,6)=LOOK_WEAPON;
		WBUFW(buf,7)=vd->weapon;
		WBUFW(buf,9)=vd->shield;
		clif_send(buf,packet_len_table[0x1d7],bl,AREA);
	}
	else if(type == LOOK_BASE && val > 255)
	{
		WBUFW(buf,0)=0x1d7;
		WBUFL(buf,2)=bl->id;
		WBUFB(buf,6)=type;
		WBUFW(buf,7)=val;
		WBUFW(buf,9)=0;
		clif_send(buf,packet_len_table[0x1d7],bl,AREA);
	} else {
		WBUFW(buf,0)=0xc3;
		WBUFL(buf,2)=bl->id;
		WBUFB(buf,6)=type;
		WBUFB(buf,7)=val;
		clif_send(buf,packet_len_table[0xc3],bl,AREA);
	}
#endif
	return 0;
}

//Sends a change-base-look packet required for traps as they are triggered.
void clif_changetraplook(struct block_list *bl,int val)
{
	unsigned char buf[32];
#if PACKETVER < 4
	WBUFW(buf,0)=0xc3;
	WBUFL(buf,2)=bl->id;
	WBUFB(buf,6)=LOOK_BASE;
	WBUFB(buf,7)=val;
	clif_send(buf,packet_len_table[0xc3],bl,AREA);
#else
	if (val > 255)
	{
		WBUFW(buf,0)=0x1d7;
		WBUFL(buf,2)=bl->id;
		WBUFB(buf,6)=LOOK_BASE;
		WBUFW(buf,7)=val;
		WBUFW(buf,9)=0;
		clif_send(buf,packet_len_table[0x1d7],bl,AREA);
	} else {
		WBUFW(buf,0)=0xc3;
		WBUFL(buf,2)=bl->id;
		WBUFB(buf,6)=LOOK_BASE;
		WBUFB(buf,7)=val;
		clif_send(buf,packet_len_table[0xc3],bl,AREA);
	}
#endif

	
}
//For the stupid cloth-dye bug. Resends the given view data
//to the area specified by bl.
void clif_refreshlook(struct block_list *bl,int id,int type,int val,int area)
{
	unsigned char buf[32];
#if PACKETVER < 4
	WBUFW(buf,0)=0xc3;
	WBUFL(buf,2)=id;
	WBUFB(buf,6)=type;
	WBUFB(buf,7)=val;
	clif_send(buf,packet_len_table[0xc3],bl,area);
#else
	if(type == LOOK_BASE && val > 255)
	{
		WBUFW(buf,0)=0x1d7;
		WBUFL(buf,2)=id;
		WBUFB(buf,6)=type;
		WBUFW(buf,7)=val;
		WBUFW(buf,9)=0;
		clif_send(buf,packet_len_table[0x1d7],bl,area);
	} else {
		WBUFW(buf,0)=0xc3;
		WBUFL(buf,2)=id;
		WBUFB(buf,6)=type;
		WBUFB(buf,7)=val;
		clif_send(buf,packet_len_table[0xc3],bl,area);
	}
#endif
	return;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_initialstatus(struct map_session_data *sd)
{
	int fd;
	unsigned char *buf;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xbd]);
	buf=WFIFOP(fd,0);

	WBUFW(buf,0)=0xbd;
	WBUFW(buf,2)=(sd->status.status_point > SHRT_MAX)? SHRT_MAX:sd->status.status_point;
	WBUFB(buf,4)=(sd->status.str > UCHAR_MAX)? UCHAR_MAX:sd->status.str;
	WBUFB(buf,5)=pc_need_status_point(sd,SP_STR);
	WBUFB(buf,6)=(sd->status.agi > UCHAR_MAX)? UCHAR_MAX:sd->status.agi;
	WBUFB(buf,7)=pc_need_status_point(sd,SP_AGI);
	WBUFB(buf,8)=(sd->status.vit > UCHAR_MAX)? UCHAR_MAX:sd->status.vit;
	WBUFB(buf,9)=pc_need_status_point(sd,SP_VIT);
	WBUFB(buf,10)=(sd->status.int_ > UCHAR_MAX)? UCHAR_MAX:sd->status.int_;
	WBUFB(buf,11)=pc_need_status_point(sd,SP_INT);
	WBUFB(buf,12)=(sd->status.dex > UCHAR_MAX)? UCHAR_MAX:sd->status.dex;
	WBUFB(buf,13)=pc_need_status_point(sd,SP_DEX);
	WBUFB(buf,14)=(sd->status.luk > UCHAR_MAX)? UCHAR_MAX:sd->status.luk;
	WBUFB(buf,15)=pc_need_status_point(sd,SP_LUK);

	WBUFW(buf,16) = sd->base_atk + sd->right_weapon.watk + sd->left_weapon.watk;
	WBUFW(buf,18) = sd->right_weapon.watk2 + sd->left_weapon.watk2; //atk bonus
	WBUFW(buf,20) = sd->matk1;
	WBUFW(buf,22) = sd->matk2;
	WBUFW(buf,24) = sd->def; // def
	WBUFW(buf,26) = sd->def2;
	WBUFW(buf,28) = sd->mdef; // mdef
	WBUFW(buf,30) = sd->mdef2;
	WBUFW(buf,32) = sd->hit;
	WBUFW(buf,34) = sd->flee;
	WBUFW(buf,36) = sd->flee2/10;
	WBUFW(buf,38) = sd->critical/10;
	WBUFW(buf,40) = sd->status.karma;
	WBUFW(buf,42) = sd->status.manner;

	WFIFOSET(fd,packet_len_table[0xbd]);

	clif_updatestatus(sd,SP_STR);
	clif_updatestatus(sd,SP_AGI);
	clif_updatestatus(sd,SP_VIT);
	clif_updatestatus(sd,SP_INT);
	clif_updatestatus(sd,SP_DEX);
	clif_updatestatus(sd,SP_LUK);

	clif_updatestatus(sd,SP_ATTACKRANGE);
	clif_updatestatus(sd,SP_ASPD);

	return 0;
}

/*==========================================
 *矢装備
 *------------------------------------------
 */
int clif_arrowequip(struct map_session_data *sd,int val)
{
	int fd;

	nullpo_retr(0, sd);

	pc_stop_attack(sd); // [Valaris]

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len_table[0x013c]);
	WFIFOW(fd,0)=0x013c;
	WFIFOW(fd,2)=val+2;//矢のアイテムID

	WFIFOSET(fd,packet_len_table[0x013c]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_arrow_fail(struct map_session_data *sd,int type)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd, packet_len_table[0x013b]);
	WFIFOW(fd,0)=0x013b;
	WFIFOW(fd,2)=type;

	WFIFOSET(fd,packet_len_table[0x013b]);

	return 0;
}

/*==========================================
 * 作成可能 矢リスト送信
 *------------------------------------------
 */
int clif_arrow_create_list(struct map_session_data *sd)
{
	int i, c, j;
	int fd;

	nullpo_retr(0, sd);

	fd = sd->fd;
	WFIFOHEAD(fd, MAX_SKILL_ARROW_DB*2+4);
	WFIFOW(fd,0) = 0x1ad;

	for (i = 0, c = 0; i < MAX_SKILL_ARROW_DB; i++) {
		if (skill_arrow_db[i].nameid > 0 &&
			(j = pc_search_inventory(sd, skill_arrow_db[i].nameid)) >= 0 &&
			!sd->status.inventory[j].equip && sd->status.inventory[j].identify)
		{
			if ((j = itemdb_viewid(skill_arrow_db[i].nameid)) > 0)
				WFIFOW(fd,c*2+4) = j;
			else
				WFIFOW(fd,c*2+4) = skill_arrow_db[i].nameid;
			c++;
		}
	}
	WFIFOW(fd,2) = c*2+4;
	WFIFOSET(fd, WFIFOW(fd,2));
	if (c > 0) {
		sd->menuskill_id = AC_MAKINGARROW;
		sd->menuskill_lv = c;
	}

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_statusupack(struct map_session_data *sd,int type,int ok,int val)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xbc]);
	WFIFOW(fd,0)=0xbc;
	WFIFOW(fd,2)=type;
	WFIFOB(fd,4)=ok;
	WFIFOB(fd,5)=val;
	WFIFOSET(fd,packet_len_table[0xbc]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_equipitemack(struct map_session_data *sd,int n,int pos,int ok)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xaa]);
	WFIFOW(fd,0)=0xaa;
	WFIFOW(fd,2)=n+2;
	WFIFOW(fd,4)=pos;
	WFIFOB(fd,6)=ok;
	WFIFOSET(fd,packet_len_table[0xaa]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_unequipitemack(struct map_session_data *sd,int n,int pos,int ok)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xac]);
	WFIFOW(fd,0)=0xac;
	WFIFOW(fd,2)=n+2;
	WFIFOW(fd,4)=pos;
	WFIFOB(fd,6)=ok;
	WFIFOSET(fd,packet_len_table[0xac]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_misceffect(struct block_list* bl,int type)
{
	unsigned char buf[32];

	nullpo_retr(0, bl);

	WBUFW(buf,0) = 0x19b;
	WBUFL(buf,2) = bl->id;
	WBUFL(buf,6) = type;

	clif_send(buf,packet_len_table[0x19b],bl,AREA);

	return 0;
}
int clif_misceffect2(struct block_list *bl, int type) {
	unsigned char buf[24];

	nullpo_retr(0, bl);

	memset(buf, 0, packet_len_table[0x1f3]);

	WBUFW(buf,0) = 0x1f3;
	WBUFL(buf,2) = bl->id;
	WBUFL(buf,6) = type;

	clif_send(buf, packet_len_table[0x1f3], bl, AREA);

	return 0;

}
/*==========================================
 * 表示オプション変更
 *------------------------------------------
 */
int clif_changeoption(struct block_list* bl)
{
	unsigned char buf[32];
	struct status_change *sc;

	nullpo_retr(0, bl);
	sc = status_get_sc(bl);
	if (!sc) return 0; //How can an option change if there's no sc?
	
#if PACKETVER > 6
	WBUFW(buf,0) = 0x229;
	WBUFL(buf,2) = bl->id;
	WBUFW(buf,6) = sc->opt1;
	WBUFW(buf,8) = sc->opt2;
	WBUFL(buf,10) = sc->option;
	WBUFB(buf,14) = 0;	// ??
	if(disguised(bl)) {
		clif_send(buf,packet_len_table[0x229],bl,AREA_WOS);
		WBUFL(buf,2) = -bl->id;
		clif_send(buf,packet_len_table[0x229],bl,SELF);
		WBUFL(buf,2) = bl->id;
		WBUFL(buf,10) = OPTION_INVISIBLE;
		clif_send(buf,packet_len_table[0x229],bl,SELF);
	} else
		clif_send(buf,packet_len_table[0x229],bl,AREA);
#else
	WBUFW(buf,0) = 0x119;
	WBUFL(buf,2) = bl->id;
	WBUFW(buf,6) = sc->opt1;
	WBUFW(buf,8) = sc->opt2;
	WBUFW(buf,10) = sc->option;
	WBUFB(buf,12) = 0;	// ??
	if(disguised(bl)) {
		clif_send(buf,packet_len_table[0x119],bl,AREA_WOS);
		WBUFL(buf,2) = -bl->id;
		clif_send(buf,packet_len_table[0x119],bl,SELF);
		WBUFL(buf,2) = bl->id;
		WBUFW(buf,10) = OPTION_INVISIBLE;
		clif_send(buf,packet_len_table[0x119],bl,SELF);
	} else
		clif_send(buf,packet_len_table[0x119],bl,AREA);
#endif

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_useitemack(struct map_session_data *sd,int index,int amount,int ok)
{
	nullpo_retr(0, sd);

	if(!ok) {
		int fd=sd->fd;
  		WFIFOHEAD(fd,packet_len_table[0xa8]);
		WFIFOW(fd,0)=0xa8;
		WFIFOW(fd,2)=index+2;
		WFIFOW(fd,4)=amount;
		WFIFOB(fd,6)=ok;
		WFIFOSET(fd,packet_len_table[0xa8]);
	}
	else {
#if PACKETVER < 3
		int fd=sd->fd;
		WFIFOHEAD(fd,packet_len_table[0xa8]);
		WFIFOW(fd,0)=0xa8;
		WFIFOW(fd,2)=index+2;
		WFIFOW(fd,4)=amount;
		WFIFOB(fd,6)=ok;
		WFIFOSET(fd,packet_len_table[0xa8]);
#else
		unsigned char buf[32];

		WBUFW(buf,0)=0x1c8;
		WBUFW(buf,2)=index+2;
		if(sd->inventory_data[index] && sd->inventory_data[index]->view_id > 0)
			WBUFW(buf,4)=sd->inventory_data[index]->view_id;
		else
			WBUFW(buf,4)=sd->status.inventory[index].nameid;
		WBUFL(buf,6)=sd->bl.id;
		WBUFW(buf,10)=amount;
		WBUFB(buf,12)=ok;
		clif_send(buf,packet_len_table[0x1c8],&sd->bl,AREA);
#endif
	}

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_createchat(struct map_session_data *sd,int fail)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xd6]);
	WFIFOW(fd,0)=0xd6;
	WFIFOB(fd,2)=fail;
	WFIFOSET(fd,packet_len_table[0xd6]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_dispchat(struct chat_data *cd,int fd)
{
	unsigned char buf[128];	// 最大title(60バイト)+17

	if(cd==NULL || *cd->owner==NULL)
		return 1;

	WBUFW(buf,0)=0xd7;
	WBUFW(buf,2)=strlen((const char*)cd->title)+17;
	WBUFL(buf,4)=(*cd->owner)->id;
	WBUFL(buf,8)=cd->bl.id;
	WBUFW(buf,12)=cd->limit;
	WBUFW(buf,14)=cd->users;
	WBUFB(buf,16)=cd->pub;
	strcpy((char*)WBUFP(buf,17),(const char*)cd->title);
	if(fd){
		WFIFOHEAD(fd, WBUFW(buf,2));
		memcpy(WFIFOP(fd,0),buf,WBUFW(buf,2));
		WFIFOSET(fd,WBUFW(buf,2));
	} else {
		clif_send(buf,WBUFW(buf,2),*cd->owner,AREA_WOSC);
	}

	return 0;
}

/*==========================================
 * chatの状態変更成功
 * 外部の人用と命令コード(d7->df)が違うだけ
 *------------------------------------------
 */
int clif_changechatstatus(struct chat_data *cd)
{
	unsigned char buf[128];	// 最大title(60バイト)+17

	if(cd==NULL || cd->usersd[0]==NULL)
		return 1;

	WBUFW(buf,0)=0xdf;
	WBUFW(buf,2)=strlen((char*)cd->title)+17;
	WBUFL(buf,4)=cd->usersd[0]->bl.id;
	WBUFL(buf,8)=cd->bl.id;
	WBUFW(buf,12)=cd->limit;
	WBUFW(buf,14)=cd->users;
	WBUFB(buf,16)=cd->pub;
	strcpy((char*)WBUFP(buf,17),(const char*)cd->title);
	clif_send(buf,WBUFW(buf,2),&cd->usersd[0]->bl,CHAT);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_clearchat(struct chat_data *cd,int fd)
{
	unsigned char buf[32];

	nullpo_retr(0, cd);

	WBUFW(buf,0)=0xd8;
	WBUFL(buf,2)=cd->bl.id;
	if(fd){
		WFIFOHEAD(fd,packet_len_table[0xd8]);
		memcpy(WFIFOP(fd,0),buf,packet_len_table[0xd8]);
		WFIFOSET(fd,packet_len_table[0xd8]);
	} else {
		clif_send(buf,packet_len_table[0xd8],*cd->owner,AREA_WOSC);
	}

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_joinchatfail(struct map_session_data *sd,int fail)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;

	WFIFOHEAD(fd,packet_len_table[0xda]);
	WFIFOW(fd,0)=0xda;
	WFIFOB(fd,2)=fail;
	WFIFOSET(fd,packet_len_table[0xda]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_joinchatok(struct map_session_data *sd,struct chat_data* cd)
{
	int fd;
	int i;

	nullpo_retr(0, sd);
	nullpo_retr(0, cd);

	fd = sd->fd;
	if (!session_isActive(fd))
		return 0;
	WFIFOHEAD(fd, 8 + (28*cd->users));
	WFIFOW(fd, 0) = 0xdb;
	WFIFOW(fd, 2) = 8 + (28*cd->users);
	WFIFOL(fd, 4) = cd->bl.id;
	for (i = 0; i < cd->users; i++) {
		WFIFOL(fd, 8+i*28) = (i!=0) || ((*cd->owner)->type == BL_NPC);
		memcpy(WFIFOP(fd, 8+i*28+4), cd->usersd[i]->status.name, NAME_LENGTH);
	}
	WFIFOSET(fd, WFIFOW(fd, 2));

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_addchat(struct chat_data* cd,struct map_session_data *sd)
{
	unsigned char buf[32];

	nullpo_retr(0, sd);
	nullpo_retr(0, cd);

	WBUFW(buf, 0) = 0x0dc;
	WBUFW(buf, 2) = cd->users;
	memcpy(WBUFP(buf, 4),sd->status.name,NAME_LENGTH);
	clif_send(buf,packet_len_table[0xdc],&sd->bl,CHAT_WOS);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_changechatowner(struct chat_data* cd,struct map_session_data *sd)
{
	unsigned char buf[64];

	nullpo_retr(0, sd);
	nullpo_retr(0, cd);

	WBUFW(buf, 0) = 0xe1;
	WBUFL(buf, 2) = 1;
	memcpy(WBUFP(buf,6),cd->usersd[0]->status.name,NAME_LENGTH);
	WBUFW(buf,30) = 0xe1;
	WBUFL(buf,32) = 0;
	memcpy(WBUFP(buf,36),sd->status.name,NAME_LENGTH);

	clif_send(buf,packet_len_table[0xe1]*2,&sd->bl,CHAT);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_leavechat(struct chat_data* cd,struct map_session_data *sd)
{
	unsigned char buf[32];

	nullpo_retr(0, sd);
	nullpo_retr(0, cd);

	WBUFW(buf, 0) = 0xdd;
	WBUFW(buf, 2) = cd->users-1;
	memcpy(WBUFP(buf,4),sd->status.name,NAME_LENGTH);
	WBUFB(buf,28) = 0;

	clif_send(buf,packet_len_table[0xdd],&sd->bl,CHAT);

	return 0;
}

/*==========================================
 * 取り引き要請受け
 *------------------------------------------
 */
int clif_traderequest(struct map_session_data *sd,char *name)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;

	WFIFOHEAD(fd,packet_len_table[0xe5]);
	WFIFOW(fd,0)=0xe5;

	strcpy((char*)WFIFOP(fd,2),name);
	
	WFIFOSET(fd,packet_len_table[0xe5]);

	return 0;
}

/*==========================================
 * 取り引き要求応答
 *------------------------------------------
 */
int clif_tradestart(struct map_session_data *sd,int type)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xe7]);
	WFIFOW(fd,0)=0xe7;
	WFIFOB(fd,2)=type;
	WFIFOSET(fd,packet_len_table[0xe7]);

	return 0;
}

/*==========================================
 * 相手方からのアイテム追加
 *------------------------------------------
 */
int clif_tradeadditem(struct map_session_data *sd,struct map_session_data *tsd,int index,int amount)
{
	int fd;

	nullpo_retr(0, sd);
	nullpo_retr(0, tsd);

	fd=tsd->fd;
	WFIFOHEAD(fd,packet_len_table[0xe9]);
	WFIFOW(fd,0)=0xe9;
	WFIFOL(fd,2)=amount;
	if(index==0){
		WFIFOW(fd,6) = 0; // type id
		WFIFOB(fd,8) = 0; //identify flag
		WFIFOB(fd,9) = 0; // attribute
		WFIFOB(fd,10)= 0; //refine
		WFIFOW(fd,11)= 0; //card (4w)
		WFIFOW(fd,13)= 0; //card (4w)
		WFIFOW(fd,15)= 0; //card (4w)
		WFIFOW(fd,17)= 0; //card (4w)
	}
	else{
		index-=2; //index fix
		if(sd->inventory_data[index] && sd->inventory_data[index]->view_id > 0)
			WFIFOW(fd,6) = sd->inventory_data[index]->view_id;
		else
			WFIFOW(fd,6) = sd->status.inventory[index].nameid; // type id
		WFIFOB(fd,8) = sd->status.inventory[index].identify; //identify flag
		WFIFOB(fd,9) = sd->status.inventory[index].attribute; // attribute
		WFIFOB(fd,10)= sd->status.inventory[index].refine; //refine
		clif_addcards(WFIFOP(fd, 11), &sd->status.inventory[index]);
	}
	WFIFOSET(fd,packet_len_table[0xe9]);

	return 0;
}

/*==========================================
 * アイテム追加成功/失敗
 *------------------------------------------
 */
int clif_tradeitemok(struct map_session_data *sd,int index,int fail)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xea]);
	WFIFOW(fd,0)=0xea;
	WFIFOW(fd,2)=index;
	WFIFOB(fd,4)=fail;
	WFIFOSET(fd,packet_len_table[0xea]);

	return 0;
}

/*==========================================
 * 取り引きok押し
 *------------------------------------------
 */
int clif_tradedeal_lock(struct map_session_data *sd,int fail)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xec]);
	WFIFOW(fd,0)=0xec;
	WFIFOB(fd,2)=fail; // 0=you 1=the other person
	WFIFOSET(fd,packet_len_table[0xec]);

	return 0;
}

/*==========================================
 * 取り引きがキャンセルされました
 *------------------------------------------
 */
int clif_tradecancelled(struct map_session_data *sd)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xee]);
	WFIFOW(fd,0)=0xee;
	WFIFOSET(fd,packet_len_table[0xee]);

	return 0;
}

/*==========================================
 * 取り引き完了
 *------------------------------------------
 */
int clif_tradecompleted(struct map_session_data *sd,int fail)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xf0]);
	WFIFOW(fd,0)=0xf0;
	WFIFOB(fd,2)=fail;
	WFIFOSET(fd,packet_len_table[0xf0]);

	return 0;
}

/*==========================================
 * カプラ倉庫のアイテム数を更新
 *------------------------------------------
 */
int clif_updatestorageamount(struct map_session_data *sd,struct storage *stor)
{
	int fd;

	nullpo_retr(0, sd);
	nullpo_retr(0, stor);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xf2]);
	WFIFOW(fd,0) = 0xf2;  // update storage amount
	WFIFOW(fd,2) = stor->storage_amount;  //items
	WFIFOW(fd,4) = MAX_STORAGE; //items max
	WFIFOSET(fd,packet_len_table[0xf2]);

	return 0;
}

/*==========================================
 * カプラ倉庫にアイテムを追加する
 *------------------------------------------
 */
int clif_storageitemadded(struct map_session_data *sd,struct storage *stor,int index,int amount)
{
	int view,fd;

	nullpo_retr(0, sd);
	nullpo_retr(0, stor);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xf4]);
	WFIFOW(fd,0) =0xf4; // Storage item added
	WFIFOW(fd,2) =index+1; // index
	WFIFOL(fd,4) =amount; // amount
	if((view = itemdb_viewid(stor->storage_[index].nameid)) > 0)
		WFIFOW(fd,8) =view;
	else
		WFIFOW(fd,8) =stor->storage_[index].nameid; // id
	WFIFOB(fd,10)=stor->storage_[index].identify; //identify flag
	WFIFOB(fd,11)=stor->storage_[index].attribute; // attribute
	WFIFOB(fd,12)=stor->storage_[index].refine; //refine
	clif_addcards(WFIFOP(fd,13), &stor->storage_[index]);
	WFIFOSET(fd,packet_len_table[0xf4]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_updateguildstorageamount(struct map_session_data *sd,struct guild_storage *stor)
{
	int fd;

	nullpo_retr(0, sd);
	nullpo_retr(0, stor);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xf2]);
	WFIFOW(fd,0) = 0xf2;  // update storage amount
	WFIFOW(fd,2) = stor->storage_amount;  //items
	WFIFOW(fd,4) = MAX_GUILD_STORAGE; //items max
	WFIFOSET(fd,packet_len_table[0xf2]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_guildstorageitemadded(struct map_session_data *sd,struct guild_storage *stor,int index,int amount)
{
	int view,fd;

	nullpo_retr(0, sd);
	nullpo_retr(0, stor);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xf4]);
	WFIFOW(fd,0) =0xf4; // Storage item added
	WFIFOW(fd,2) =index+1; // index
	WFIFOL(fd,4) =amount; // amount
	if((view = itemdb_viewid(stor->storage_[index].nameid)) > 0)
		WFIFOW(fd,8) =view;
	else
		WFIFOW(fd,8) =stor->storage_[index].nameid; // id
	WFIFOB(fd,10)=stor->storage_[index].identify; //identify flag
	WFIFOB(fd,11)=stor->storage_[index].attribute; // attribute
	WFIFOB(fd,12)=stor->storage_[index].refine; //refine
	clif_addcards(WFIFOP(fd,13), &stor->storage_[index]);
	WFIFOSET(fd,packet_len_table[0xf4]);

	return 0;
}

/*==========================================
 * カプラ倉庫からアイテムを取り去る
 *------------------------------------------
 */
int clif_storageitemremoved(struct map_session_data *sd,int index,int amount)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xf6]);
	WFIFOW(fd,0)=0xf6; // Storage item removed
	WFIFOW(fd,2)=index+1;
	WFIFOL(fd,4)=amount;
	WFIFOSET(fd,packet_len_table[0xf6]);

	return 0;
}

/*==========================================
 * カプラ倉庫を閉じる
 *------------------------------------------
 */
int clif_storageclose(struct map_session_data *sd)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xf8]);
	WFIFOW(fd,0)=0xf8; // Storage Closed
	WFIFOSET(fd,packet_len_table[0xf8]);

	return 0;
}

//
// callback系 ?
//
/*==========================================
 * PC表示
 *------------------------------------------
 */
void clif_getareachar_pc(struct map_session_data* sd,struct map_session_data* dstsd)
{
	int len;
	if(dstsd->chatID){
		struct chat_data *cd;
		cd=(struct chat_data*)map_id2bl(dstsd->chatID);
		if(cd && cd->usersd[0]==dstsd)
			clif_dispchat(cd,sd->fd);
	}
	if(dstsd->vender_id)
		clif_showvendingboard(&dstsd->bl,dstsd->message,sd->fd);

	if(dstsd->spiritball > 0)
		clif_spiritball_single(sd->fd, dstsd);

	if((sd->status.party_id && dstsd->status.party_id == sd->status.party_id) || //Party-mate, or hpdisp setting.
		(battle_config.disp_hpmeter && (len = pc_isGM(sd)) >= battle_config.disp_hpmeter && len >= pc_isGM(dstsd))
		)
		clif_hpmeter_single(sd->fd, dstsd);

	if(dstsd->status.manner < 0 && battle_config.manner_system)
		clif_changestatus(&dstsd->bl,SP_MANNER,dstsd->status.manner);
		
	// pvp circle for duel [LuzZza]
	/*
	if(dstsd->duel_group)
		clif_specialeffect(&dstsd->bl, 159, 4);
	*/

}
void clif_getareachar_char(struct map_session_data* sd,struct block_list *bl)
{
	struct unit_data *ud;
	struct view_data *vd;
	int len;
	
	vd = status_get_viewdata(bl);

	if (!vd || vd->class_ == INVISIBLE_CLASS)
		return;

	ud = unit_bl2ud(bl);
	if (ud && ud->walktimer != -1)
	{
#if PACKETVER < 4
		WFIFOHEAD(sd->fd, packet_len_table[0x7b]);
#else
		WFIFOHEAD(sd->fd, packet_len_table[0x1da]);
#endif
		len = clif_set007b(bl,vd,ud,WFIFOP(sd->fd,0));
		WFIFOSET(sd->fd,len);
	} else {
#if PACKETVER < 4
		WFIFOHEAD(sd->fd,packet_len_table[0x78]);
#else
		WFIFOHEAD(sd->fd,packet_len_table[0x1d8]);
#endif
		len = clif_set0078(bl,vd,WFIFOP(sd->fd,0));
		WFIFOSET(sd->fd,len);
	}

	if (vd->cloth_color)
		clif_refreshlook(&sd->bl,bl->id,LOOK_CLOTHES_COLOR,vd->cloth_color,SELF);

	switch (bl->type)
	{
	case BL_PC:
		{
			TBL_PC* tsd = (TBL_PC*)bl;
			clif_getareachar_pc(sd, tsd);
			if(tsd->state.size==2) // tiny/big players [Valaris]
				clif_specialeffect(bl,423,0);
			else if(tsd->state.size==1)
				clif_specialeffect(bl,421,0);
		}
		break;
	case BL_NPC:
		{
			if(((TBL_NPC*)bl)->chat_id)
				clif_dispchat((struct chat_data*)map_id2bl(((TBL_NPC*)bl)->chat_id),sd->fd);
		}
		break;
	case BL_MOB:
		{
			TBL_MOB* md = (TBL_MOB*)bl;
			if(md->special_state.size==2) // tiny/big mobs [Valaris]
				clif_specialeffect(bl,423,0);
			else if(md->special_state.size==1)
				clif_specialeffect(bl,421,0);
		}
		break;
	}
}

/*==========================================
 * Older fix pos packet.
 *------------------------------------------
 */
int clif_fixpos2(struct block_list* bl)
{
	struct unit_data *ud;
	struct view_data *vd;
	unsigned char buf[256];
	int len;

	nullpo_retr(0, bl);
	ud = unit_bl2ud(bl);
	vd = status_get_viewdata(bl);
	if (!vd || vd->class_ == INVISIBLE_CLASS)
		return 0;
	
	if(ud && ud->walktimer != -1)
		len = clif_set007b(bl,vd,ud,buf);
	else
		len = clif_set0078(bl,vd,buf);

	if (disguised(bl)) {
		clif_send(buf,len,bl,AREA_WOS);
		clif_setdisguise((TBL_PC*)bl, buf, len, 0);
		clif_setdisguise((TBL_PC*)bl, buf, len, 1);
	} else
		clif_send(buf,len,bl,AREA);
	return 0;
}

//Modifies the type of damage according to status changes [Skotlex]
#define clif_calc_delay(type,delay) (type==1||type==4||type==0x0a)?type:(delay==0?9:type)

/*==========================================
 * Estimates walk delay based on the damage criteria. [Skotlex]
 *------------------------------------------
 */
static int clif_calc_walkdelay(struct block_list *bl,int delay, int type, int damage, int div_) {

	if (type == 4 || type == 9 || damage <=0)
		return 0;
	
	if (bl->type == BL_PC) {
		if (battle_config.pc_walk_delay_rate != 100)
			delay = delay*battle_config.pc_walk_delay_rate/100;
	} else
		if (battle_config.walk_delay_rate != 100)
			delay = delay*battle_config.walk_delay_rate/100;
	
	if (div_ > 1) //Multi-hit skills mean higher delays.
		delay += battle_config.multihit_delay*(div_-1);

	return delay>0?delay:1; //Return 1 to specify there should be no noticeable delay, but you should stop walking.
}

/*==========================================
 * 通常攻撃エフェクト＆ダメージ
 *------------------------------------------
 */
int clif_damage(struct block_list *src,struct block_list *dst,unsigned int tick,int sdelay,int ddelay,int damage,int div,int type,int damage2)
{
	unsigned char buf[256];
	struct status_change *sc;

	nullpo_retr(0, src);
	nullpo_retr(0, dst);

	type = clif_calc_delay(type, ddelay); //Type defaults to 0 for normal attacks.

	sc = status_get_sc(dst);

	if(sc && sc->count) {
		if(sc->data[SC_HALLUCINATION].timer != -1) {
			if(damage > 0)
				damage = damage*(5+sc->data[SC_HALLUCINATION].val1) + rand()%100;
			if(damage2 > 0)
				damage2 = damage2*(5+sc->data[SC_HALLUCINATION].val1) + rand()%100;
		}
	}

	WBUFW(buf,0)=0x8a;
	WBUFL(buf,2)=src->id;
	WBUFL(buf,6)=dst->id;
	WBUFL(buf,10)=tick;
	WBUFL(buf,14)=sdelay;
	WBUFL(buf,18)=ddelay;
	if (battle_config.hide_woe_damage && map_flag_gvg(src->m)) {
		WBUFW(buf,22)=div;
		WBUFW(buf,27)=damage2?div:0;
	} else {
		WBUFW(buf,22)=(damage > SHRT_MAX)?SHRT_MAX:damage;
		WBUFW(buf,27)=damage2;
	}
	WBUFW(buf,24)=div;
	WBUFB(buf,26)=type;
	clif_send(buf,packet_len_table[0x8a],src,AREA);

	if(disguised(src)) {
		WBUFL(buf,2)=-src->id;
		if(damage > 0)
			WBUFW(buf,22)=-1;
		if(damage2 > 0)
			WBUFW(buf,27)=-1;
		clif_send(buf,packet_len_table[0x8a],src,SELF);
	}
	if (disguised(dst)) {
		WBUFL(buf,6)=-dst->id;
		if (disguised(src))
			WBUFL(buf,2)=src->id;
		else {
			if(damage > 0)
				WBUFW(buf,22)=-1;
			if(damage2 > 0)
				WBUFW(buf,27)=-1;
		}
		clif_send(buf,packet_len_table[0x8a],dst,SELF);
	}
	//Return adjusted can't walk delay for further processing.
	return clif_calc_walkdelay(dst,ddelay,type,damage+damage2,div);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_getareachar_item(struct map_session_data* sd,struct flooritem_data* fitem)
{
	int view,fd;
	fd=sd->fd;
	//009d <ID>.l <item ID>.w <identify flag>.B <X>.w <Y>.w <amount>.w <subX>.B <subY>.B
	WFIFOHEAD(fd,packet_len_table[0x9d]);

	WFIFOW(fd,0)=0x9d;
	WFIFOL(fd,2)=fitem->bl.id;
	if((view = itemdb_viewid(fitem->item_data.nameid)) > 0)
		WFIFOW(fd,6)=view;
	else
		WFIFOW(fd,6)=fitem->item_data.nameid;
	WFIFOB(fd,8)=fitem->item_data.identify;
	WFIFOW(fd,9)=fitem->bl.x;
	WFIFOW(fd,11)=fitem->bl.y;
	WFIFOW(fd,13)=fitem->item_data.amount;
	WFIFOB(fd,15)=fitem->subx;
	WFIFOB(fd,16)=fitem->suby;
	WFIFOSET(fd,packet_len_table[0x9d]);
}

/*==========================================
 * 場所スキルエフェクトが視界に入る
 *------------------------------------------
 */
int clif_getareachar_skillunit(struct map_session_data *sd,struct skill_unit *unit)
{
	int fd;
	struct block_list *bl;

	fd=sd->fd;
	bl=map_id2bl(unit->group->src_id);
#if PACKETVER >= 3
	if(unit->group->unit_id==UNT_GRAFFITI)	{ // Graffiti [Valaris]
		WFIFOHEAD(fd,packet_len_table[0x1c9]);
		memset(WFIFOP(fd,0),0,packet_len_table[0x1c9]);
		WFIFOW(fd, 0)=0x1c9;
		WFIFOL(fd, 2)=unit->bl.id;
		WFIFOL(fd, 6)=unit->group->src_id;
		WFIFOW(fd,10)=unit->bl.x;
		WFIFOW(fd,12)=unit->bl.y; // might be typo? [Lance]
		WFIFOB(fd,14)=unit->group->unit_id;
		WFIFOB(fd,15)=1;
		WFIFOB(fd,16)=1;
		memcpy(WFIFOP(fd,17),unit->group->valstr,MESSAGE_SIZE);
		WFIFOSET(fd,packet_len_table[0x1c9]);
		return 0;
	}
#endif
	WFIFOHEAD(fd,packet_len_table[0x11f]);
	memset(WFIFOP(fd,0),0,packet_len_table[0x11f]);
	WFIFOW(fd, 0)=0x11f;
	WFIFOL(fd, 2)=unit->bl.id;
	WFIFOL(fd, 6)=unit->group->src_id;
	WFIFOW(fd,10)=unit->bl.x;
	WFIFOW(fd,12)=unit->bl.y;
	//Use invisible unit id for traps.
	if (battle_config.traps_setting&1 && skill_get_inf2(unit->group->skill_id)&INF2_TRAP)
		WFIFOB(fd,14)=UNT_ATTACK_SKILLS;
	else
		WFIFOB(fd,14)=unit->group->unit_id;
	WFIFOB(fd,15)=0;
	WFIFOSET(fd,packet_len_table[0x11f]);

	if(unit->group->skill_id == WZ_ICEWALL)
		clif_set0192(fd,unit->bl.m,unit->bl.x,unit->bl.y,5);
	return 0;
/* Previous implementation guess of packet 0x1c9, who can understand what all those fields are for? [Skotlex]
	WFIFOHEAD(fd,packet_len_table[0x1c9]);
	memset(WFIFOP(fd,0),0,packet_len_table[0x1c9]);
	WFIFOW(fd, 0)=0x1c9;
	WFIFOL(fd, 2)=unit->bl.id;
	WFIFOL(fd, 6)=unit->group->src_id;
	WFIFOW(fd,10)=unit->bl.x;
	WFIFOW(fd,12)=unit->bl.y;
	WFIFOB(fd,14)=unit->group->unit_id;
	WFIFOB(fd,15)=1;
	if(unit->group->unit_id==UNT_GRAFFITI)	{ // Graffiti [Valaris]
		WFIFOB(fd,16)=1;
		memcpy(WFIFOP(fd,17),unit->group->valstr,MESSAGE_SIZE);
	} else {
		WFIFOL(fd,15+1)=0;						//1-4調べた限り固定
		WFIFOL(fd,15+5)=0;						//5-8調べた限り固定
												//9-12マップごとで一定の77-80とはまた違う4バイトのかなり大きな数字
		WFIFOL(fd,15+13)=unit->bl.y - 0x12;		//13-16ユニットのY座標-18っぽい(Y:17でFF FF FF FF)
		WFIFOL(fd,15+17)=0x004f37dd;			//17-20調べた限り固定
		WFIFOL(fd,15+21)=0x0012f674;			//21-24調べた限り固定
		WFIFOL(fd,15+25)=0x0012f664;			//25-28調べた限り固定
		WFIFOL(fd,15+29)=0x0012f654;			//29-32調べた限り固定
		WFIFOL(fd,15+33)=0x77527bbc;			//33-36調べた限り固定
												//37-39
		WFIFOB(fd,15+40)=0x2d;					//40調べた限り固定
		WFIFOL(fd,15+41)=0;						//41-44調べた限り0固定
		WFIFOL(fd,15+45)=0;						//45-48調べた限り0固定
		WFIFOL(fd,15+49)=0;						//49-52調べた限り0固定
		WFIFOL(fd,15+53)=0x0048d919;			//53-56調べた限り固定
		WFIFOL(fd,15+57)=0x0000003e;			//57-60調べた限り固定
		WFIFOL(fd,15+61)=0x0012f66c;			//61-64調べた限り固定
												//65-68
												//69-72
		if(bl) WFIFOL(fd,15+73)=bl->y;			//73-76術者のY座標
		WFIFOL(fd,15+77)=unit->bl.m;			//77-80マップIDかなぁ？かなり2バイトで足りそうな数字
		WFIFOB(fd,15+81)=0xaa;					//81終端文字0xaa
	}

	WFIFOSET(fd,packet_len_table[0x1c9]);
#endif
	if(unit->group->skill_id == WZ_ICEWALL)
		clif_set0192(fd,unit->bl.m,unit->bl.x,unit->bl.y,5);

	return 0;
*/
}

/*==========================================
 * 場所スキルエフェクトが視界から消える
 *------------------------------------------
 */
int clif_clearchar_skillunit(struct skill_unit *unit,int fd)
{
	nullpo_retr(0, unit);

	WFIFOHEAD(fd,packet_len_table[0x120]);
	WFIFOW(fd, 0)=0x120;
	WFIFOL(fd, 2)=unit->bl.id;
	WFIFOSET(fd,packet_len_table[0x120]);
	if(unit->group && unit->group->skill_id == WZ_ICEWALL)
		clif_set0192(fd,unit->bl.m,unit->bl.x,unit->bl.y,unit->val2);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_01ac(struct block_list *bl)
{
	unsigned char buf[32];

	nullpo_retr(0, bl);

	WBUFW(buf, 0) = 0x1ac;
	WBUFL(buf, 2) = bl->id;

	clif_send(buf,packet_len_table[0x1ac],bl,AREA);
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
 int clif_getareachar(struct block_list* bl,va_list ap)
{
	struct map_session_data *sd;

	nullpo_retr(0, bl);
	nullpo_retr(0, ap);

	sd=va_arg(ap,struct map_session_data*);

	if (sd == NULL || session[sd->fd] == NULL)
		return 0;

	switch(bl->type){
	case BL_ITEM:
		clif_getareachar_item(sd,(struct flooritem_data*) bl);
		break;
	case BL_SKILL:
		clif_getareachar_skillunit(sd,(TBL_SKILL*)bl);
		break;
	default:
		if(&sd->bl == bl)
			break;
		clif_getareachar_char(sd,bl);
		break;
	}
	return 0;
}

/*==========================================
 * tbl has gone out of view-size of bl
 *------------------------------------------
 */
int clif_outsight(struct block_list *bl,va_list ap)
{
	struct block_list *tbl;
	struct view_data *vd;
	TBL_PC *sd, *tsd;
	tbl=va_arg(ap,struct block_list*);
	if(bl == tbl) return 0;
	BL_CAST(BL_PC, bl, sd);
	BL_CAST(BL_PC, tbl, tsd);

	if (tsd && tsd->fd)
	{	//tsd has lost sight of the bl object.
		switch(bl->type){
		case BL_PC:
			if (((TBL_PC*)bl)->vd.class_ != INVISIBLE_CLASS)
				clif_clearchar_id(bl->id,0,tsd->fd);
			if(sd->chatID){
				struct chat_data *cd;
				cd=(struct chat_data*)map_id2bl(sd->chatID);
				if(cd->usersd[0]==sd)
					clif_dispchat(cd,tsd->fd);
			}
			if(sd->vender_id)
				clif_closevendingboard(bl,tsd->fd);
			break;
		case BL_ITEM:
			clif_clearflooritem((struct flooritem_data*)bl,tsd->fd);
			break;
		case BL_SKILL:
			clif_clearchar_skillunit((struct skill_unit *)bl,tsd->fd);
			break;
		default:
			if ((vd=status_get_viewdata(bl)) && vd->class_ != INVISIBLE_CLASS)
				clif_clearchar_id(bl->id,0,tsd->fd);
			break;
		}
	}
	if (sd && sd->fd)
	{	//sd is watching tbl go out of view.
		if ((vd=status_get_viewdata(tbl)) && vd->class_ != INVISIBLE_CLASS)
			clif_clearchar_id(tbl->id,0,sd->fd);
	}
	return 0;
}

/*==========================================
 * tbl has come into view of bl
 *------------------------------------------
 */
int clif_insight(struct block_list *bl,va_list ap)
{
	struct block_list *tbl;
	TBL_PC *sd, *tsd;
	tbl=va_arg(ap,struct block_list*);

	if (bl == tbl) return 0;
	
	BL_CAST(BL_PC, bl, sd);
	BL_CAST(BL_PC, tbl, tsd);

	if (tsd && tsd->fd)
	{	//Tell tsd that bl entered into his view
		switch(bl->type){
		case BL_ITEM:
			clif_getareachar_item(tsd,(struct flooritem_data*)bl);
			break;
		case BL_SKILL:
			clif_getareachar_skillunit(tsd,(TBL_SKILL*)bl);
			break;
		default:
			clif_getareachar_char(tsd,bl);
			break;
		}
	}
	if (sd && sd->fd)
	{	//Tell sd that tbl walked into his view
		clif_getareachar_char(sd,tbl);
	}
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_skillinfo(struct map_session_data *sd,int skillid,int type,int range)
{
	int fd,id, inf2;

	nullpo_retr(0, sd);

	fd=sd->fd;
	if( (id=sd->status.skill[skillid].id) <= 0 )
		return 0;
	WFIFOHEAD(fd,packet_len_table[0x147]);
	WFIFOW(fd,0)=0x147;
	WFIFOW(fd,2) = id;
	if(type < 0)
		WFIFOW(fd,4) = skill_get_inf(id);
	else
		WFIFOW(fd,4) = type;
	WFIFOW(fd,6) = 0;
	WFIFOW(fd,8) = sd->status.skill[skillid].lv;
	WFIFOW(fd,10) = skill_get_sp(id,sd->status.skill[skillid].lv);
	if(range < 0)
		range = skill_get_range2(&sd->bl, id,sd->status.skill[skillid].lv);

	WFIFOW(fd,12)= range;
	strncpy(WFIFOP(fd,14), skill_get_name(id), NAME_LENGTH);
	inf2 = skill_get_inf2(id);
	if(((!(inf2&INF2_QUEST_SKILL) || battle_config.quest_skill_learn) &&
		!(inf2&(INF2_WEDDING_SKILL|INF2_SPIRIT_SKILL))) ||
		(battle_config.gm_allskill > 0 && pc_isGM(sd) >= battle_config.gm_allskill))
		//WFIFOB(fd,38)= (sd->status.skill[skillid].lv < skill_get_max(id) && sd->status.skill[skillid].flag ==0 )? 1:0;
		WFIFOB(fd,38)= (sd->status.skill[skillid].lv < skill_tree_get_max(id, sd->status.class_) && sd->status.skill[skillid].flag ==0 )? 1:0;
	else
		WFIFOB(fd,38) = 0;
	WFIFOSET(fd,packet_len_table[0x147]);

	return 0;
}

/*==========================================
 * スキルリストを送信する
 *------------------------------------------
 */
int clif_skillinfoblock(struct map_session_data *sd)
{
	int fd;
	int i,c,len=4,id, inf2;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd, MAX_SKILL * 37 + 4);
	WFIFOW(fd,0)=0x10f;
	for ( i = c = 0; i < MAX_SKILL; i++){
		if( (id=sd->status.skill[i].id)!=0 ){
			WFIFOW(fd,len  ) = id;
			WFIFOW(fd,len+2) = skill_get_inf(id);
			WFIFOW(fd,len+4) = 0;
			WFIFOW(fd,len+6) = sd->status.skill[i].lv;
			WFIFOW(fd,len+8) = skill_get_sp(id,sd->status.skill[i].lv);
			WFIFOW(fd,len+10)= skill_get_range2(&sd->bl, id,sd->status.skill[i].lv);
			strncpy(WFIFOP(fd,len+12), skill_get_name(id), NAME_LENGTH);
			inf2 = skill_get_inf2(id);
			if(((!(inf2&INF2_QUEST_SKILL) || battle_config.quest_skill_learn) &&
				!(inf2&(INF2_WEDDING_SKILL|INF2_SPIRIT_SKILL))) ||
				(battle_config.gm_allskill > 0 && pc_isGM(sd) >= battle_config.gm_allskill) )
				//WFIFOB(fd,len+36)= (sd->status.skill[i].lv < skill_get_max(id) && sd->status.skill[i].flag ==0 )? 1:0;
				WFIFOB(fd,len+36)= (sd->status.skill[i].lv < skill_tree_get_max(id, sd->status.class_) && sd->status.skill[i].flag ==0 )? 1:0;
			else
				WFIFOB(fd,len+36) = 0;
			len+=37;
			c++;
		}
	}
	WFIFOW(fd,2)=len;
	WFIFOSET(fd,len);

	return 0;
}

/*==========================================
 * スキル割り振り通知
 *------------------------------------------
 */
int clif_skillup(struct map_session_data *sd,int skill_num)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x10e]);
	WFIFOW(fd,0) = 0x10e;
	WFIFOW(fd,2) = skill_num;
	WFIFOW(fd,4) = sd->status.skill[skill_num].lv;
	WFIFOW(fd,6) = skill_get_sp(skill_num,sd->status.skill[skill_num].lv);
	WFIFOW(fd,8) = skill_get_range2(&sd->bl,skill_num,sd->status.skill[skill_num].lv);
	//WFIFOB(fd,10) = (sd->status.skill[skill_num].lv < skill_get_max(sd->status.skill[skill_num].id)) ? 1 : 0;
	WFIFOB(fd,10) = (sd->status.skill[skill_num].lv < skill_tree_get_max(sd->status.skill[skill_num].id, sd->status.class_)) ? 1 : 0;
	WFIFOSET(fd,packet_len_table[0x10e]);

	return 0;
}

/*==========================================
 * スキル詠唱エフェクトを送信する
 *------------------------------------------
 */
int clif_skillcasting(struct block_list* bl,
	int src_id,int dst_id,int dst_x,int dst_y,int skill_num,int casttime)
{
	int pl = skill_get_pl(skill_num);
	unsigned char buf[32];
	WBUFW(buf,0) = 0x13e;
	WBUFL(buf,2) = src_id;
	WBUFL(buf,6) = dst_id;
	WBUFW(buf,10) = dst_x;
	WBUFW(buf,12) = dst_y;
	WBUFW(buf,14) = skill_num;
	WBUFL(buf,16) = pl<0?0:pl; //Avoid sending negatives as element [Skotlex]
	WBUFL(buf,20) = casttime;
	clif_send(buf,packet_len_table[0x13e], bl, AREA);
	if (disguised(bl)) {
		WBUFL(buf,2) = -src_id;
		WBUFL(buf,20) = 0;
		clif_send(buf,packet_len_table[0x13e], bl, SELF);
	}

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_skillcastcancel(struct block_list* bl)
{
	unsigned char buf[16];

	nullpo_retr(0, bl);

	WBUFW(buf,0) = 0x1b9;
	WBUFL(buf,2) = bl->id;
	clif_send(buf,packet_len_table[0x1b9], bl, AREA);

	return 0;
}

/*==========================================
 * スキル詠唱失敗
 *------------------------------------------
 */
int clif_skill_fail(struct map_session_data *sd,int skill_id,int type,int btype)
{
	int fd;

	if (!sd) {	//Since this is the most common nullpo.... 
		ShowDebug("clif_skill_fail: Error, received NULL sd for skill %d\n", skill_id);
		return 0;
	}
	
	fd=sd->fd;

	// reset all variables [celest]
	// Should be handled now by the unit_* code.
	sd->skillitem = sd->skillitemlv = -1;

	if(type==0x4 && !sd->state.showdelay)
		return 0;

	WFIFOHEAD(fd,packet_len_table[0x110]);
	WFIFOW(fd,0) = 0x110;
	WFIFOW(fd,2) = skill_id;
	WFIFOW(fd,4) = btype;
	WFIFOW(fd,6) = 0;
	WFIFOB(fd,8) = 0;
	WFIFOB(fd,9) = type;
	WFIFOSET(fd,packet_len_table[0x110]);

	return 0;
}

/*==========================================
 * スキル攻撃エフェクト＆ダメージ
 *------------------------------------------
 */
int clif_skill_damage(struct block_list *src,struct block_list *dst,
	unsigned int tick,int sdelay,int ddelay,int damage,int div,int skill_id,int skill_lv,int type)
{
	unsigned char buf[64];
	struct status_change *sc;

	nullpo_retr(0, src);
	nullpo_retr(0, dst);

	type = (type>0)?type:skill_get_hit(skill_id);
	type = clif_calc_delay(type, ddelay);
	sc = status_get_sc(dst);

	if(sc && sc->count) {
		if(sc->data[SC_HALLUCINATION].timer != -1 && damage > 0)
			damage = damage*(5+sc->data[SC_HALLUCINATION].val1) + rand()%100;
	}

#if PACKETVER < 3
	WBUFW(buf,0)=0x114;
	WBUFW(buf,2)=skill_id;
	WBUFL(buf,4)=src->id;
	WBUFL(buf,8)=dst->id;
	WBUFL(buf,12)=tick;
	WBUFL(buf,16)=sdelay;
	WBUFL(buf,20)=ddelay;
	if (battle_config.hide_woe_damage && map_flag_gvg(src->m)) {
		WBUFW(buf,24)=div;
	} else {
		WBUFW(buf,24)=damage;
	}
	WBUFW(buf,26)=skill_lv;
	WBUFW(buf,28)=div;
	WBUFB(buf,30)=type;
	clif_send(buf,packet_len_table[0x114],src,AREA);
	if(disguised(src)) {
		WBUFL(buf,4)=-src->id;
		if(damage > 0)
			WBUFW(buf,24)=-1;
		clif_send(buf,packet_len_table[0x114],src,SELF);
	}
	if (disguised(dst)) {
		WBUFL(buf,8)=-dst->id;
		if (disguised(src))
			WBUFL(buf,4)=src->id;
		else if(damage > 0)
			WBUFW(buf,24)=-1;
		clif_send(buf,packet_len_table[0x114],dst,SELF);
	}
#else
	WBUFW(buf,0)=0x1de;
	WBUFW(buf,2)=skill_id;
	WBUFL(buf,4)=src->id;
	WBUFL(buf,8)=dst->id;
	WBUFL(buf,12)=tick;
	WBUFL(buf,16)=sdelay;
	WBUFL(buf,20)=ddelay;
	if (battle_config.hide_woe_damage && map_flag_gvg(src->m)) {
		WBUFL(buf,24)=div;
	} else {
		WBUFL(buf,24)=damage;
	}
	WBUFW(buf,28)=skill_lv;
	WBUFW(buf,30)=div;
	WBUFB(buf,32)=type;
	clif_send(buf,packet_len_table[0x1de],src,AREA);
	if(disguised(src)) {
		WBUFL(buf,4)=-src->id;
		if(damage > 0)
			WBUFW(buf,24)=-1;
		clif_send(buf,packet_len_table[0x1de],src,SELF);
	}
	if (disguised(dst)) {
		WBUFL(buf,8)=-dst->id;
		if (disguised(src))
			WBUFL(buf,4)=src->id;
		else if(damage > 0)
			WBUFW(buf,24)=-1;
		clif_send(buf,packet_len_table[0x1de],dst,SELF);
	}
#endif

	//Because the damage delay must be synced with the client, here is where the can-walk tick must be updated. [Skotlex]
	return clif_calc_walkdelay(dst,ddelay,type,damage,div);
}

/*==========================================
 * 吹き飛ばしスキル攻撃エフェクト＆ダメージ
 *------------------------------------------
 */
int clif_skill_damage2(struct block_list *src,struct block_list *dst,
	unsigned int tick,int sdelay,int ddelay,int damage,int div,int skill_id,int skill_lv,int type)
{
	unsigned char buf[64];
	struct status_change *sc;

	nullpo_retr(0, src);
	nullpo_retr(0, dst);

	type = (type>0)?type:skill_get_hit(skill_id);
	type = clif_calc_delay(type, ddelay);
	sc = status_get_sc(dst);

	if(sc && sc->count) {
		if(sc->data[SC_HALLUCINATION].timer != -1 && damage > 0)
			damage = damage*(5+sc->data[SC_HALLUCINATION].val1) + rand()%100;
	}

	WBUFW(buf,0)=0x115;
	WBUFW(buf,2)=skill_id;
	WBUFL(buf,4)=src->id;
	WBUFL(buf,8)=dst->id;
	WBUFL(buf,12)=tick;
	WBUFL(buf,16)=sdelay;
	WBUFL(buf,20)=ddelay;
	WBUFW(buf,24)=dst->x;
	WBUFW(buf,26)=dst->y;
	if (battle_config.hide_woe_damage && map_flag_gvg(src->m)) {
		WBUFW(buf,28)=div;
	} else {
		WBUFW(buf,28)=damage;
	}
	WBUFW(buf,30)=skill_lv;
	WBUFW(buf,32)=div;
	WBUFB(buf,34)=type;
	clif_send(buf,packet_len_table[0x115],src,AREA);
	if(disguised(src)) {
		WBUFL(buf,4)=-src->id;
		if(damage > 0)
			WBUFW(buf,28)=-1;
		clif_send(buf,packet_len_table[0x115],src,SELF);
	}
	if (disguised(dst)) {
		WBUFL(buf,8)=-dst->id;
		if (disguised(src))
			WBUFL(buf,4)=src->id;
		else if(damage > 0)
			WBUFW(buf,28)=-1;
		clif_send(buf,packet_len_table[0x115],dst,SELF);
	}

	//Because the damage delay must be synced with the client, here is where the can-walk tick must be updated. [Skotlex]
	return clif_calc_walkdelay(dst,ddelay,type,damage,div);
}

/*==========================================
 * 支援/回復スキルエフェクト
 *------------------------------------------
 */
int clif_skill_nodamage(struct block_list *src,struct block_list *dst,
	int skill_id,int heal,int fail)
{
	unsigned char buf[32];

	nullpo_retr(0, dst);

	WBUFW(buf,0)=0x11a;
	WBUFW(buf,2)=skill_id;
	WBUFW(buf,4)=(heal > SHRT_MAX)? SHRT_MAX:heal;
	WBUFL(buf,6)=dst->id;
	WBUFL(buf,10)=src?src->id:0;
	WBUFB(buf,14)=fail;
	clif_send(buf,packet_len_table[0x11a],dst,AREA);

	if (disguised(dst)) {
		WBUFL(buf,6)=-dst->id;
		clif_send(buf,packet_len_table[0x115],dst,SELF);
	}

	if(src && disguised(src)) {
		WBUFL(buf,10)=-src->id;
		if (disguised(dst))
			WBUFL(buf,6)=dst->id;
		clif_send(buf,packet_len_table[0x115],src,SELF);
	}

	return fail;
}

/*==========================================
 * 場所スキルエフェクト
 *------------------------------------------
 */
int clif_skill_poseffect(struct block_list *src,int skill_id,int val,int x,int y,int tick)
{
	unsigned char buf[32];

	nullpo_retr(0, src);

	WBUFW(buf,0)=0x117;
	WBUFW(buf,2)=skill_id;
	WBUFL(buf,4)=src->id;
	WBUFW(buf,8)=val;
	WBUFW(buf,10)=x;
	WBUFW(buf,12)=y;
	WBUFL(buf,14)=tick;
	clif_send(buf,packet_len_table[0x117],src,AREA);
	if(disguised(src)) {
		WBUFL(buf,4)=-src->id;
		clif_send(buf,packet_len_table[0x117],src,SELF);
	}

	return 0;
}

/*==========================================
 * 場所スキルエフェクト表示
 *------------------------------------------
 */
int clif_skill_setunit(struct skill_unit *unit)
{
	unsigned char buf[128];
	struct block_list *bl;

	nullpo_retr(0, unit);

	bl=map_id2bl(unit->group->src_id);

// These are invisible client-side, but are necessary because
// otherwise the client will not know who caused the attack.
//	if (unit->group->unit_id == UNT_ATTACK_SKILLS)
//		return 0;
		
#if PACKETVER >= 3
	if(unit->group->unit_id==UNT_GRAFFITI)	{ // Graffiti [Valaris]
		memset(WBUFP(buf, 0),0,packet_len_table[0x1c9]);
		WBUFW(buf, 0)=0x1c9;
		WBUFL(buf, 2)=unit->bl.id;
		WBUFL(buf, 6)=unit->group->src_id;
		WBUFW(buf,10)=unit->bl.x;
		WBUFW(buf,12)=unit->bl.y;
		WBUFB(buf,14)=unit->group->unit_id;
		WBUFB(buf,15)=1;
		WBUFB(buf,16)=1;
		memcpy(WBUFP(buf,17),unit->group->valstr,MESSAGE_SIZE);
		clif_send(buf,packet_len_table[0x1c9],&unit->bl,AREA);
		return 0;
	}
#endif
	memset(WBUFP(buf, 0),0,packet_len_table[0x11f]);
	WBUFW(buf, 0)=0x11f;
	WBUFL(buf, 2)=unit->bl.id;
	WBUFL(buf, 6)=unit->group->src_id;
	WBUFW(buf,10)=unit->bl.x;
	WBUFW(buf,12)=unit->bl.y;
	WBUFB(buf,14)=unit->group->unit_id;
	WBUFB(buf,15)=0;
	clif_send(buf,packet_len_table[0x11f],&unit->bl,AREA);
	return 0;
	
/* Previous mysterious implementation noone really understands. [Skotlex]
		memset(WBUFP(buf, 0),0,packet_len_table[0x1c9]);
		WBUFW(buf, 0)=0x1c9;
		WBUFL(buf, 2)=unit->bl.id;
		WBUFL(buf, 6)=unit->group->src_id;
		WBUFW(buf,10)=unit->bl.x;
		WBUFW(buf,12)=unit->bl.y;
		WBUFB(buf,14)=unit->group->unit_id;
		WBUFB(buf,15)=1;
		if(unit->group->unit_id==0xb0)	{ // Graffiti [Valaris]
			WBUFB(buf,16)=1;
			memcpy(WBUFP(buf,17),unit->group->valstr,MESSAGE_SIZE);
		} else {
			WBUFL(buf,15+1)=0;						//1-4調べた限り固定
			WBUFL(buf,15+5)=0;						//5-8調べた限り固定
												//9-12マップごとで一定の77-80とはまた違う4バイトのかなり大きな数字
			WBUFL(buf,15+13)=unit->bl.y - 0x12;		//13-16ユニットのY座標-18っぽい(Y:17でFF FF FF FF)
			WBUFL(buf,15+17)=0x004f37dd;			//17-20調べた限り固定(0x1b2で0x004fdbddだった)
			WBUFL(buf,15+21)=0x0012f674;			//21-24調べた限り固定
			WBUFL(buf,15+25)=0x0012f664;			//25-28調べた限り固定
			WBUFL(buf,15+29)=0x0012f654;			//29-32調べた限り固定
			WBUFL(buf,15+33)=0x77527bbc;			//33-36調べた限り固定
												//37-39
			WBUFB(buf,15+40)=0x2d;					//40調べた限り固定
			WBUFL(buf,15+41)=0;						//41-44調べた限り0固定
			WBUFL(buf,15+45)=0;						//45-48調べた限り0固定
			WBUFL(buf,15+49)=0;						//49-52調べた限り0固定
			WBUFL(buf,15+53)=0x0048d919;			//53-56調べた限り固定(0x01b2で0x00495119だった)
			WBUFL(buf,15+57)=0x0000003e;			//57-60調べた限り固定
			WBUFL(buf,15+61)=0x0012f66c;			//61-64調べた限り固定
												//65-68
												//69-72
			if(bl) WBUFL(buf,15+73)=bl->y;			//73-76術者のY座標
				WBUFL(buf,15+77)=unit->bl.m;			//77-80マップIDかなぁ？かなり2バイトで足りそうな数字
			WBUFB(buf,15+81)=0xaa;					//81終端文字0xaa
		}
		clif_send(buf,packet_len_table[0x1c9],&unit->bl,AREA);
#endif
	return 0;
*/
}
/*==========================================
 * 場所スキルエフェクト削除
 *------------------------------------------
 */
int clif_skill_delunit(struct skill_unit *unit)
{
	unsigned char buf[16];

	nullpo_retr(0, unit);

	WBUFW(buf, 0)=0x120;
	WBUFL(buf, 2)=unit->bl.id;
	clif_send(buf,packet_len_table[0x120],&unit->bl,AREA);
	return 0;
}
/*==========================================
 * ワープ場所選択
 *------------------------------------------
 */
int clif_skill_warppoint(struct map_session_data *sd,int skill_num,int skill_lv,
	const char *map1,const char *map2,const char *map3,const char *map4)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x11c]);
	WFIFOW(fd,0)=0x11c;
	WFIFOW(fd,2)=skill_num;
	strncpy((char*)WFIFOP(fd, 4),map1,MAP_NAME_LENGTH);
	strncpy((char*)WFIFOP(fd,20),map2,MAP_NAME_LENGTH);
	strncpy((char*)WFIFOP(fd,36),map3,MAP_NAME_LENGTH);
	strncpy((char*)WFIFOP(fd,52),map4,MAP_NAME_LENGTH);
	WFIFOSET(fd,packet_len_table[0x11c]);
	sd->menuskill_id = skill_num;
	if (skill_num == AL_WARP)
		sd->menuskill_lv = (sd->ud.skillx<<16)|sd->ud.skilly; //Store warp position here.
	else
		sd->menuskill_lv = skill_lv;
	return 0;
}
/*==========================================
 * メモ応答
 *------------------------------------------
 */
int clif_skill_memo(struct map_session_data *sd,int flag)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;

	WFIFOHEAD(fd,packet_len_table[0x11e]);
	WFIFOW(fd,0)=0x11e;
	WFIFOB(fd,2)=flag;
	WFIFOSET(fd,packet_len_table[0x11e]);
	return 0;
}
int clif_skill_teleportmessage(struct map_session_data *sd,int flag)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x189]);
	WFIFOW(fd,0)=0x189;
	WFIFOW(fd,2)=flag;
	WFIFOSET(fd,packet_len_table[0x189]);
	return 0;
}

/*==========================================
 * モンスター情報
 *------------------------------------------
 */
int clif_skill_estimation(struct map_session_data *sd,struct block_list *dst)
{
	struct mob_data *md;
	unsigned char buf[64];
	int i;//, fix;

	nullpo_retr(0, sd);
	nullpo_retr(0, dst);

	if(dst->type!=BL_MOB )
		return 0;
	if((md=(struct mob_data *)dst) == NULL)
		return 0;

	WBUFW(buf, 0)=0x18c;
	WBUFW(buf, 2)=md->vd->class_;
	WBUFW(buf, 4)=md->level;
	WBUFW(buf, 6)=md->db->size;
	WBUFL(buf, 8)=md->hp;
	WBUFW(buf,12)= (battle_config.estimation_type&1?status_get_def(&md->bl):0)
		+(battle_config.estimation_type&2?status_get_def2(&md->bl):0);
	WBUFW(buf,14)=md->db->race;
	WBUFW(buf,16)= (battle_config.estimation_type&1?status_get_mdef(&md->bl):0)
  		+(battle_config.estimation_type&2?status_get_mdef2(&md->bl) - (md->db->vit>>1):0);
	WBUFW(buf,18)=status_get_elem_type(&md->bl);
	for(i=0;i<9;i++)
		WBUFB(buf,20+i)= (unsigned char)battle_attr_fix(NULL,dst,100,i+1,md->def_ele);
//		The following caps negative attributes to 0 since the client displays them as 255-fix. [Skotlex]
//		WBUFB(buf,20+i)= (unsigned char)((fix=battle_attr_fix(NULL,dst,100,i+1,md->def_ele))<0?0:fix);

	if(sd->status.party_id>0)
		clif_send(buf,packet_len_table[0x18c],&sd->bl,PARTY_AREA);
	else{
		WFIFOHEAD(sd->fd,packet_len_table[0x18c]);
		memcpy(WFIFOP(sd->fd,0),buf,packet_len_table[0x18c]);
		WFIFOSET(sd->fd,packet_len_table[0x18c]);
	}
	return 0;
}
/*==========================================
 * アイテム合成可能リスト
 *------------------------------------------
 */
int clif_skill_produce_mix_list(struct map_session_data *sd, int trigger)
{
	int i,c,view,fd;
	nullpo_retr(0, sd);

	if(sd->menuskill_id == AM_PHARMACY)
		return 0; //Avoid resending the menu twice or more times...
	fd=sd->fd;
	WFIFOHEAD(fd, MAX_SKILL_PRODUCE_DB * 8 + 8);
	WFIFOW(fd, 0)=0x18d;

	for(i=0,c=0;i<MAX_SKILL_PRODUCE_DB;i++){
		if( skill_can_produce_mix(sd,skill_produce_db[i].nameid,trigger, 1) ){
			if((view = itemdb_viewid(skill_produce_db[i].nameid)) > 0)
				WFIFOW(fd,c*8+ 4)= view;
			else
				WFIFOW(fd,c*8+ 4)= skill_produce_db[i].nameid;
			WFIFOW(fd,c*8+ 6)= 0x0012;
			WFIFOL(fd,c*8+ 8)= sd->status.char_id;
			c++;
		}
	}
	WFIFOW(fd, 2)=c*8+8;
	WFIFOSET(fd,WFIFOW(fd,2));
	if(c > 0) {
		sd->menuskill_id = AM_PHARMACY;
		sd->menuskill_lv = trigger;
		return 1;
	}
	return 0;
}

/*==========================================
 * Sends a status change packet to the object only, used for loading status changes. [Skotlex]
 *------------------------------------------
 */
int clif_status_load(struct block_list *bl,int type, int flag)
{
	int fd;
	if (type == SI_BLANK)  //It shows nothing on the client...
		return 0;
	
	if (bl->type != BL_PC)
		return 0;

	fd = ((struct map_session_data*)bl)->fd;
	
	WFIFOHEAD(fd,packet_len_table[0x196]);
	WFIFOW(fd,0)=0x0196;
	WFIFOW(fd,2)=type;
	WFIFOL(fd,4)=bl->id;
	WFIFOB(fd,8)=flag; //Status start
	WFIFOSET(fd, packet_len_table[0x196]);
	return 0;
}
/*==========================================
 * 状態異常アイコン/メッセージ表示
 *------------------------------------------
 */
int clif_status_change(struct block_list *bl,int type,int flag)
{
	unsigned char buf[16];

	if (type == SI_BLANK)  //It shows nothing on the client...
		return 0;
	
	nullpo_retr(0, bl);

	WBUFW(buf,0)=0x0196;
	WBUFW(buf,2)=type;
	WBUFL(buf,4)=bl->id;
	WBUFB(buf,8)=flag;
	clif_send(buf,packet_len_table[0x196],bl,AREA);
	return 0;
}

/*==========================================
 * Send message (modified by [Yor])
 *------------------------------------------
 */
int clif_displaymessage(const int fd, char* mes)
{
	// invalid pointer?
	nullpo_retr(-1, mes);
	
	//Console [Wizputer] //Scrapped, as these are shared by disconnected players =X [Skotlex]
	if (fd == 0)
		return 0;
	else {
		int len_mes = strlen(mes);

		if (len_mes > 0) { // don't send a void message (it's not displaying on the client chat). @help can send void line.
                        WFIFOHEAD(fd, 5 + len_mes);
			WFIFOW(fd,0) = 0x8e;
			WFIFOW(fd,2) = 5 + len_mes; // 4 + len + NULL teminate
			memcpy(WFIFOP(fd,4), mes, len_mes + 1);
			WFIFOSET(fd, 5 + len_mes);
		}
	}

	return 0;
}

/*==========================================
 * 天の声を送信する
 *------------------------------------------
 */
int clif_GMmessage(struct block_list *bl, char* mes, int len, int flag)
{
	unsigned char *buf;
	int lp;

	lp = (flag & 0x10) ? 8 : 4;
	buf = (unsigned char*)aMallocA((len + lp + 8)*sizeof(unsigned char));

	WBUFW(buf,0) = 0x9a;
	WBUFW(buf,2) = len + lp;
	WBUFL(buf,4) = 0x65756c62;
	memcpy(WBUFP(buf,lp), mes, len);
	flag &= 0x07;
	clif_send(buf, WBUFW(buf,2), bl,
	          (flag == 1) ? ALL_SAMEMAP :
	          (flag == 2) ? AREA :
	          (flag == 3) ? SELF :
	          ALL_CLIENT);
	if(buf) aFree(buf);

	if(use_irc && irc_announce_flag && !flag)
		irc_announce(mes);

	return 0;
}

/*==========================================
 * グローバルメッセージ
 *------------------------------------------
 */
void clif_GlobalMessage(struct block_list *bl,char *message)
{
	char buf[100];
	int len;

	nullpo_retv(bl);

	if(!message)
		return;

	len = strlen(message)+1;

	WBUFW(buf,0)=0x8d;
	WBUFW(buf,2)=len+8;
	WBUFL(buf,4)=bl->id;
	strncpy((char *) WBUFP(buf,8),message,len);
	clif_send((unsigned char *) buf,WBUFW(buf,2),bl,ALL_CLIENT);
}

/*==========================================
 * Send main chat message [LuzZza]
 *------------------------------------------
 */
void clif_MainChatMessage(char* message) {

	char buf[200];
	int len;
	
	if(!message)
		return;
		
	len = strlen(message)+1;
	if (len+8 > sizeof(buf)) {
		ShowDebug("clif_MainChatMessage: Received message too long (len %d): %s\n", len, message);
		len = sizeof(buf)-8;
	}
	WBUFW(buf,0)=0x8d;
	WBUFW(buf,2)=len+8;
	WBUFL(buf,4)=0;
	strncpy((char *) WBUFP(buf,8),message,len);
	clif_send((unsigned char *) buf,WBUFW(buf,2),NULL,CHAT_MAINCHAT);
}

/*==========================================
 * Does an announce message in the given color. 
 *------------------------------------------
 */
int clif_announce(struct block_list *bl, char* mes, int len, unsigned long color, int flag)
{
	unsigned char *buf;
	buf = (unsigned char*)aMallocA((len + 16)*sizeof(unsigned char));
	WBUFW(buf,0) = 0x1c3;
	WBUFW(buf,2) = len + 16;
	WBUFL(buf,4) = color;
	WBUFW(buf,8) = 0x190; //Font style? Type?
	WBUFW(buf,10) = 0x0c;  //12? Font size?
	WBUFL(buf,12) = 0;	//Unknown!
	memcpy(WBUFP(buf,16), mes, len);
	
	flag &= 0x07;
	clif_send(buf, WBUFW(buf,2), bl,
	          (flag == 1) ? ALL_SAMEMAP :
	          (flag == 2) ? AREA :
	          (flag == 3) ? SELF :
	          ALL_CLIENT);

	if(buf) aFree(buf);
	return 0;
}
/*==========================================
 * HPSP回復エフェクトを送信する
 *------------------------------------------
 */
int clif_heal(int fd,int type,int val)
{
	WFIFOHEAD(fd,packet_len_table[0x13d]);
	WFIFOW(fd,0)=0x13d;
	WFIFOW(fd,2)=type;
	WFIFOW(fd,4)=val;
	WFIFOSET(fd,packet_len_table[0x13d]);

	return 0;
}

/*==========================================
 * 復活する
 *------------------------------------------
 */
int clif_resurrection(struct block_list *bl,int type)
{
	unsigned char buf[16];

	nullpo_retr(0, bl);

	WBUFW(buf,0)=0x148;
	WBUFL(buf,2)=bl->id;
	WBUFW(buf,6)=type;

	clif_send(buf,packet_len_table[0x148],bl,type==1 ? AREA : AREA_WOS);
	if (disguised(bl))
		clif_spawn(bl);

	return 0;
}

/*==========================================
 * PVP実装？（仮）
 *------------------------------------------
 */
int clif_set0199(int fd,int type)
{
	WFIFOHEAD(fd,packet_len_table[0x199]);
	WFIFOW(fd,0)=0x199;
	WFIFOW(fd,2)=type;
	WFIFOSET(fd,packet_len_table[0x199]);

	return 0;
}

/*==========================================
 * PVP実装？(仮)
 *------------------------------------------
 */
int clif_pvpset(struct map_session_data *sd,int pvprank,int pvpnum,int type)
{
	nullpo_retr(0, sd);

	if(type == 2) {
		WFIFOHEAD(sd->fd,packet_len_table[0x19a]);
		WFIFOW(sd->fd,0) = 0x19a;
		WFIFOL(sd->fd,2) = sd->bl.id;
		if(pvprank<=0)
			pc_calc_pvprank(sd);
		WFIFOL(sd->fd,6) = pvprank;
		WFIFOL(sd->fd,10) = pvpnum;
		WFIFOSET(sd->fd,packet_len_table[0x19a]);
	} else {
		unsigned char buf[32];

		WBUFW(buf,0) = 0x19a;
		WBUFL(buf,2) = sd->bl.id;
		if(sd->sc.option&0x46)
		// WTF? a -1 to an unsigned value...
			WBUFL(buf,6) = 0xFFFFFFFF;
		else
			if(pvprank<=0)
				pc_calc_pvprank(sd);
			WBUFL(buf,6) = pvprank;
		WBUFL(buf,10) = pvpnum;
		if(!type)
			clif_send(buf,packet_len_table[0x19a],&sd->bl,AREA);
		else
			clif_send(buf,packet_len_table[0x19a],&sd->bl,ALL_SAMEMAP);
	}

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_send0199(int map,int type)
{
	struct block_list bl;
	unsigned char buf[16];

	bl.type = BL_NUL;
	bl.m = map;
	WBUFW(buf,0)=0x199;
	WBUFW(buf,2)=type;
	clif_send(buf,packet_len_table[0x199],&bl,ALL_SAMEMAP);

	return 0;
}

/*==========================================
 * 精錬エフェクトを送信する
 *------------------------------------------
 */
int clif_refine(int fd,struct map_session_data *sd,int fail,int index,int val)
{
	WFIFOHEAD(fd,packet_len_table[0x188]);
	WFIFOW(fd,0)=0x188;
	WFIFOW(fd,2)=fail;
	WFIFOW(fd,4)=index+2;
	WFIFOW(fd,6)=val;
	WFIFOSET(fd,packet_len_table[0x188]);

	return 0;
}

/*==========================================
 * Wisp/page is transmitted to the destination player
 *------------------------------------------
 */
int clif_wis_message(int fd, char *nick, char *mes, int mes_len) // R 0097 <len>.w <nick>.24B <message>.?B
{
//	printf("clif_wis_message(%d, %s, %s)\n", fd, nick, mes);

	WFIFOHEAD(fd, mes_len + NAME_LENGTH + 4);
	WFIFOW(fd,0) = 0x97;
	WFIFOW(fd,2) = mes_len + NAME_LENGTH + 4;
	memcpy(WFIFOP(fd,4), nick, NAME_LENGTH);
	memcpy(WFIFOP(fd,28), mes, mes_len);
	WFIFOSET(fd,WFIFOW(fd,2));
	return 0;
}

/*==========================================
 * The transmission result of Wisp/page is transmitted to the source player
 *------------------------------------------
 */
int clif_wis_end(int fd, int flag) // R 0098 <type>.B: 0: success to send wisper, 1: target character is not loged in, 2: ignored by target, 3: everyone ignored by target
{
	WFIFOHEAD(fd,packet_len_table[0x98]);
	WFIFOW(fd,0) = 0x98;
	WFIFOW(fd,2) = flag;
	WFIFOSET(fd,packet_len_table[0x98]);
	return 0;
}

/*==========================================
 * キャラID名前引き結果を送信する
 *------------------------------------------
 */
int clif_solved_charname(struct map_session_data *sd,int char_id)
{
	char *p= map_charid2nick(char_id);
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	if(p!=NULL){
                WFIFOHEAD(fd,packet_len_table[0x194]);
		WFIFOW(fd,0)=0x194;
		WFIFOL(fd,2)=char_id;
		memcpy(WFIFOP(fd,6), p, NAME_LENGTH);
		WFIFOSET(fd,packet_len_table[0x194]);
	}else{
		map_reqchariddb(sd,char_id);
		chrif_searchcharid(char_id);
	}
	return 0;
}

/*==========================================
 * カードの挿入可能リストを返す
 *------------------------------------------
 */
int clif_use_card(struct map_session_data *sd,int idx)
{
	int i,c,ep;
	int fd=sd->fd;

	nullpo_retr(0, sd);
	if (idx < 0 || idx >= MAX_INVENTORY) //Crash-fix from bad packets.
		return 0;

	if (!sd->inventory_data[idx] || sd->inventory_data[idx]->type != 6)
		return 0; //Avoid parsing invalid item indexes (no card/no item)
			
	ep=sd->inventory_data[idx]->equip;
	WFIFOHEAD(fd,MAX_INVENTORY * 2 + 4);
	WFIFOW(fd,0)=0x017b;

	for(i=c=0;i<MAX_INVENTORY;i++){
		int j;

		if(sd->inventory_data[i] == NULL)
			continue;
		if(sd->inventory_data[i]->type!=4 && sd->inventory_data[i]->type!=5)	// 武器防具じゃない
			continue;
		if(sd->status.inventory[i].card[0]==0x00ff || sd->status.inventory[i].card[0]==(short)0xff00 || sd->status.inventory[i].card[0]==0x00fe)
			continue;
		if(sd->status.inventory[i].identify==0 )	// 未鑑定
			continue;

		if((sd->inventory_data[i]->equip&ep)==0)	// 装備個所が違う
			continue;
		if(sd->inventory_data[i]->type==4 && ep==32)	// 盾カードと両手武器
			continue;
		for(j=0;j<sd->inventory_data[i]->slot;j++){
			if( sd->status.inventory[i].card[j]==0 )
				break;
		}
		if(j==sd->inventory_data[i]->slot)	// すでにカードが一杯
			continue;

		WFIFOW(fd,4+c*2)=i+2;
		c++;
	}
	WFIFOW(fd,2)=4+c*2;
	WFIFOSET(fd,WFIFOW(fd,2));

	return 0;
}
/*==========================================
 * カードの挿入終了
 *------------------------------------------
 */
int clif_insert_card(struct map_session_data *sd,int idx_equip,int idx_card,int flag)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x17d]);
	WFIFOW(fd,0)=0x17d;
	WFIFOW(fd,2)=idx_equip+2;
	WFIFOW(fd,4)=idx_card+2;
	WFIFOB(fd,6)=flag;
	WFIFOSET(fd,packet_len_table[0x17d]);
	return 0;
}

/*==========================================
 * 鑑定可能アイテムリスト送信
 *------------------------------------------
 */
int clif_item_identify_list(struct map_session_data *sd)
{
	int i,c;
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;

	WFIFOHEAD(fd,MAX_INVENTORY * 2 + 4);
	WFIFOW(fd,0)=0x177;
	for(i=c=0;i<MAX_INVENTORY;i++){
		if(sd->status.inventory[i].nameid > 0 && !sd->status.inventory[i].identify){
			WFIFOW(fd,c*2+4)=i+2;
			c++;
		}
	}
	if(c > 0) {
		WFIFOW(fd,2)=c*2+4;
		WFIFOSET(fd,WFIFOW(fd,2));
		sd->menuskill_id = MC_IDENTIFY;
		sd->menuskill_lv = c;
	}
	return 0;
}

/*==========================================
 * 鑑定結果
 *------------------------------------------
 */
int clif_item_identified(struct map_session_data *sd,int idx,int flag)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x179]);
	WFIFOW(fd, 0)=0x179;
	WFIFOW(fd, 2)=idx+2;
	WFIFOB(fd, 4)=flag;
	WFIFOSET(fd,packet_len_table[0x179]);
	return 0;
}

/*==========================================
 * 修理可能アイテムリスト送信
 *------------------------------------------
 */
int clif_item_repair_list(struct map_session_data *sd,struct map_session_data *dstsd)
{
	int i,c;
	int fd;
	int nameid;

	nullpo_retr(0, sd);
	nullpo_retr(0, dstsd);

	fd=sd->fd;

	WFIFOHEAD(fd, MAX_INVENTORY * 13 + 4);
	WFIFOW(fd,0)=0x1fc;
	for(i=c=0;i<MAX_INVENTORY;i++){
		if((nameid=dstsd->status.inventory[i].nameid) > 0 && dstsd->status.inventory[i].attribute!=0){// && skill_can_repair(sd,nameid)){
			WFIFOW(fd,c*13+4) = i;
			WFIFOW(fd,c*13+6) = nameid;
			WFIFOL(fd,c*13+8) = sd->status.char_id;
			WFIFOL(fd,c*13+12)= dstsd->status.char_id;
			WFIFOB(fd,c*13+16)= c;
			c++;
		}
	}
	if(c > 0) {
		WFIFOW(fd,2)=c*13+4;
		WFIFOSET(fd,WFIFOW(fd,2));
		sd->menuskill_id = BS_REPAIRWEAPON;
		sd->menuskill_lv = dstsd->bl.id;
	}else
		clif_skill_fail(sd,sd->ud.skillid,0,0);

	return 0;
}
int clif_item_repaireffect(struct map_session_data *sd,int nameid,int flag)
{
	int view,fd;

	nullpo_retr(0, sd);
	fd=sd->fd;

	WFIFOHEAD(fd,packet_len_table[0x1fe]);
	WFIFOW(fd, 0)=0x1fe;
	if((view = itemdb_viewid(nameid)) > 0)
		WFIFOW(fd, 2)=view;
	else
		WFIFOW(fd, 2)=nameid;
	WFIFOB(fd, 4)=flag;
	WFIFOSET(fd,packet_len_table[0x1fe]);

	return 0;
}

/*==========================================
 * Weapon Refining - Taken from jAthena
 *------------------------------------------
 */
int clif_item_refine_list(struct map_session_data *sd)
{
	int i,c;
	int fd;
	int skilllv;
	int wlv;
	int refine_item[5];

	nullpo_retr(0, sd);

	skilllv = pc_checkskill(sd,WS_WEAPONREFINE);

	fd=sd->fd;

	refine_item[0] = -1;
	refine_item[1] = pc_search_inventory(sd,1010);
	refine_item[2] = pc_search_inventory(sd,1011);
	refine_item[3] = refine_item[4] = pc_search_inventory(sd,984);

	WFIFOHEAD(fd, MAX_INVENTORY * 13 + 4);
	WFIFOW(fd,0)=0x221;
	for(i=c=0;i<MAX_INVENTORY;i++){
		if(sd->status.inventory[i].nameid > 0 && sd->status.inventory[i].refine < skilllv &&
			sd->status.inventory[i].identify && (wlv=itemdb_wlv(sd->status.inventory[i].nameid)) >=1 &&
			refine_item[wlv]!=-1 && !(sd->status.inventory[i].equip&0x0022)){
			WFIFOW(fd,c*13+ 4)=i+2;
			WFIFOW(fd,c*13+ 6)=sd->status.inventory[i].nameid;
			WFIFOW(fd,c*13+ 8)=0; //TODO: Wonder what are these for? Perhaps ID of weapon's crafter if any?
			WFIFOW(fd,c*13+10)=0;
			WFIFOB(fd,c*13+12)=c;
			c++;
		}
	}
	WFIFOW(fd,2)=c*13+4;
	WFIFOSET(fd,WFIFOW(fd,2));
	sd->menuskill_id = WS_WEAPONREFINE;
	sd->menuskill_lv = skilllv;

	return 0;
}

/*==========================================
 * アイテムによる一時的なスキル効果
 *------------------------------------------
 */
int clif_item_skill(struct map_session_data *sd,int skillid,int skilllv,const char *name)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x147]);
	WFIFOW(fd, 0)=0x147;
	WFIFOW(fd, 2)=skillid;
	WFIFOW(fd, 4)=skill_get_inf(skillid);
	WFIFOW(fd, 6)=0;
	WFIFOW(fd, 8)=skilllv;
	WFIFOW(fd,10)=skill_get_sp(skillid,skilllv);
	WFIFOW(fd,12)=skill_get_range2(&sd->bl, skillid,skilllv);
	strncpy((char*)WFIFOP(fd,14),name,NAME_LENGTH);
	WFIFOB(fd,38)=0;
	WFIFOSET(fd,packet_len_table[0x147]);
	return 0;
}

/*==========================================
 * カートにアイテム追加
 *------------------------------------------
 */
int clif_cart_additem(struct map_session_data *sd,int n,int amount,int fail)
{
	int view,fd;
	unsigned char *buf;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x124]);
	buf=WFIFOP(fd,0);
	if(n<0 || n>=MAX_CART || sd->status.cart[n].nameid<=0)
		return 1;

	WBUFW(buf,0)=0x124;
	WBUFW(buf,2)=n+2;
	WBUFL(buf,4)=amount;
	if((view = itemdb_viewid(sd->status.cart[n].nameid)) > 0)
		WBUFW(buf,8)=view;
	else
		WBUFW(buf,8)=sd->status.cart[n].nameid;
	WBUFB(buf,10)=sd->status.cart[n].identify;
	WBUFB(buf,11)=sd->status.cart[n].attribute;
	WBUFB(buf,12)=sd->status.cart[n].refine;
	clif_addcards(WBUFP(buf,13), &sd->status.cart[n]);
	WFIFOSET(fd,packet_len_table[0x124]);
	return 0;
}

/*==========================================
 * カートからアイテム削除
 *------------------------------------------
 */
int clif_cart_delitem(struct map_session_data *sd,int n,int amount)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;

	WFIFOHEAD(fd,packet_len_table[0x125]);
	WFIFOW(fd,0)=0x125;
	WFIFOW(fd,2)=n+2;
	WFIFOL(fd,4)=amount;

	WFIFOSET(fd,packet_len_table[0x125]);

	return 0;
}

/*==========================================
 * カートのアイテムリスト
 *------------------------------------------
 */
int clif_cart_itemlist(struct map_session_data *sd)
{
	struct item_data *id;
	int i,n,fd;
	unsigned char *buf;

	nullpo_retr(0, sd);

	fd=sd->fd;
        WFIFOHEAD(fd, MAX_CART * 18 + 4);
	buf = WFIFOP(fd,0);
#if PACKETVER < 5
	for(i=0,n=0;i<MAX_CART;i++){
		if(sd->status.cart[i].nameid<=0)
			continue;
		id = itemdb_search(sd->status.cart[i].nameid);
		if(itemdb_isequip2(id))
			continue;
		WBUFW(buf,n*10+4)=i+2;
		if(id->view_id > 0)
			WBUFW(buf,n*10+6)=id->view_id;
		else
			WBUFW(buf,n*10+6)=sd->status.cart[i].nameid;
		WBUFB(buf,n*10+8)=itemtype(id->type);
		WBUFB(buf,n*10+9)=sd->status.cart[i].identify;
		WBUFW(buf,n*10+10)=sd->status.cart[i].amount;
		WBUFW(buf,n*10+12)=0;
		n++;
	}
	if(n){
		WBUFW(buf,0)=0x123;
		WBUFW(buf,2)=4+n*10;
		WFIFOSET(fd,WFIFOW(fd,2));
	}
#else
	for(i=0,n=0;i<MAX_CART;i++){
		if(sd->status.cart[i].nameid<=0)
			continue;
		id = itemdb_search(sd->status.cart[i].nameid);
		if(itemdb_isequip2(id))
			continue;
		WBUFW(buf,n*18+4)=i+2;
		if(id->view_id > 0)
			WBUFW(buf,n*18+6)=id->view_id;
		else
			WBUFW(buf,n*18+6)=sd->status.cart[i].nameid;
		WBUFB(buf,n*18+8)=itemtype(id->type);
		WBUFB(buf,n*18+9)=sd->status.cart[i].identify;
		WBUFW(buf,n*18+10)=sd->status.cart[i].amount;
		WBUFW(buf,n*18+12)=0; //Here goes the equip location, which seems unnecessary to fill for the cart data.
		clif_addcards(WBUFP(buf,n*18+14), &sd->status.cart[i]);
		n++;
	}
	if(n){
		WBUFW(buf,0)=0x1ef;
		WBUFW(buf,2)=4+n*18;
		WFIFOSET(fd,WFIFOW(fd,2));
	}
#endif
	return 0;
}

/*==========================================
 * カートの装備品リスト
 *------------------------------------------
 */
int clif_cart_equiplist(struct map_session_data *sd)
{
	struct item_data *id;
	int i,n,fd;
	unsigned char *buf;

	nullpo_retr(0, sd);

	fd=sd->fd;
        WFIFOHEAD(fd, MAX_INVENTORY * 20 + 4);
	buf = WFIFOP(fd,0);

	for(i=0,n=0;i<MAX_INVENTORY;i++){
		if(sd->status.cart[i].nameid<=0)
			continue;
		id = itemdb_search(sd->status.cart[i].nameid);
		if(!itemdb_isequip2(id))
			continue;
		WBUFW(buf,n*20+4)=i+2;
		if(id->view_id > 0)
			WBUFW(buf,n*20+6)=id->view_id;
		else
			WBUFW(buf,n*20+6)=sd->status.cart[i].nameid;
		WBUFB(buf,n*20+8)=itemtype(id->type);
		WBUFB(buf,n*20+9)=sd->status.cart[i].identify;
		WBUFW(buf,n*20+10)=id->equip;
		WBUFW(buf,n*20+12)=sd->status.cart[i].equip;
		WBUFB(buf,n*20+14)=sd->status.cart[i].attribute;
		WBUFB(buf,n*20+15)=sd->status.cart[i].refine;
		clif_addcards(WBUFP(buf, n*20+16), &sd->status.cart[i]);
		n++;
	}
	if(n){
		WBUFW(buf,0)=0x122;
		WBUFW(buf,2)=4+n*20;
		WFIFOSET(fd,WFIFOW(fd,2));
	}
	return 0;
}

/*==========================================
 * 露店開設
 *------------------------------------------
 */
int clif_openvendingreq(struct map_session_data *sd,int num)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x12d]);
	WFIFOW(fd,0)=0x12d;
	WFIFOW(fd,2)=num;
	WFIFOSET(fd,packet_len_table[0x12d]);

	return 0;
}

/*==========================================
 * 露店看板表示
 *------------------------------------------
 */
int clif_showvendingboard(struct block_list* bl,char *message,int fd)
{
	unsigned char buf[128];

	nullpo_retr(0, bl);

	WBUFW(buf,0)=0x131;
	WBUFL(buf,2)=bl->id;
	strncpy((char*)WBUFP(buf,6),message,80);
	if(fd){
		WFIFOHEAD(fd,packet_len_table[0x131]);
		memcpy(WFIFOP(fd,0),buf,packet_len_table[0x131]);
		WFIFOSET(fd,packet_len_table[0x131]);
	}else{
		clif_send(buf,packet_len_table[0x131],bl,AREA_WOS);
	}
	return 0;
}

/*==========================================
 * 露店看板消去
 *------------------------------------------
 */
int clif_closevendingboard(struct block_list* bl,int fd)
{
	unsigned char buf[16];

	nullpo_retr(0, bl);

	WBUFW(buf,0)=0x132;
	WBUFL(buf,2)=bl->id;
	if(fd){
		WFIFOHEAD(fd,packet_len_table[0x132]);
		memcpy(WFIFOP(fd,0),buf,packet_len_table[0x132]);
		WFIFOSET(fd,packet_len_table[0x132]);
	}else{
		clif_send(buf,packet_len_table[0x132],bl,AREA_WOS);
	}

	return 0;
}
/*==========================================
 * 露店アイテムリスト
 *------------------------------------------
 */
int clif_vendinglist(struct map_session_data *sd,int id,struct vending *vending)
{
	struct item_data *data;
	int i,n,index,fd;
	struct map_session_data *vsd;
	unsigned char *buf;

	nullpo_retr(0, sd);
	nullpo_retr(0, vending);
	nullpo_retr(0, vsd=map_id2sd(id));

	fd=sd->fd;
        WFIFOHEAD(fd, 8+vsd->vend_num*22);
	buf = WFIFOP(fd,0);
	for(i=0,n=0;i<vsd->vend_num;i++){
		if(vending[i].amount<=0)
			continue;
		WBUFL(buf,8+n*22)=vending[i].value;
		WBUFW(buf,12+n*22)=vending[i].amount;
		WBUFW(buf,14+n*22)=(index=vending[i].index)+2;
		if(vsd->status.cart[index].nameid <= 0 || vsd->status.cart[index].amount <= 0)
			continue;
		data = itemdb_search(vsd->status.cart[index].nameid);
		WBUFB(buf,16+n*22)=itemtype(data->type);
		if(data->view_id > 0)
			WBUFW(buf,17+n*22)=data->view_id;
		else
			WBUFW(buf,17+n*22)=vsd->status.cart[index].nameid;
		WBUFB(buf,19+n*22)=vsd->status.cart[index].identify;
		WBUFB(buf,20+n*22)=vsd->status.cart[index].attribute;
		WBUFB(buf,21+n*22)=vsd->status.cart[index].refine;
		clif_addcards(WBUFP(buf, 22+n*22), &vsd->status.cart[index]);
		n++;
	}
	if(n > 0){
		WBUFW(buf,0)=0x133;
		WBUFW(buf,2)=8+n*22;
		WBUFL(buf,4)=id;
		WFIFOSET(fd,WFIFOW(fd,2));
	}

	return 0;
}

/*==========================================
 * 露店アイテム購入失敗
 *------------------------------------------
*/
int clif_buyvending(struct map_session_data *sd,int index,int amount,int fail)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x135]);
	WFIFOW(fd,0)=0x135;
	WFIFOW(fd,2)=index+2;
	WFIFOW(fd,4)=amount;
	WFIFOB(fd,6)=fail;
	WFIFOSET(fd,packet_len_table[0x135]);

	return 0;
}

/*==========================================
 * 露店開設成功
 *------------------------------------------
*/
int clif_openvending(struct map_session_data *sd,int id,struct vending *vending)
{
	struct item_data *data;
	int i,n,index,fd;
	unsigned char *buf;

	nullpo_retr(0, sd);

	fd=sd->fd;
        WFIFOHEAD(fd, 8+sd->vend_num*22);
	buf = WFIFOP(fd,0);
	for(i=0,n=0;i<sd->vend_num;i++){
		if (sd->vend_num > 2+pc_checkskill(sd,MC_VENDING)) return 0;
		WBUFL(buf,8+n*22)=vending[i].value;
		WBUFW(buf,12+n*22)=(index=vending[i].index)+2;
		WBUFW(buf,14+n*22)=vending[i].amount;
		if(sd->status.cart[index].nameid <= 0 || sd->status.cart[index].amount <= 0 || !sd->status.cart[index].identify ||
			sd->status.cart[index].attribute==1) // Prevent unidentified and broken items from being sold [Valaris]
			continue;
		data = itemdb_search(sd->status.cart[index].nameid);
		WBUFB(buf,16+n*22)=itemtype(data->type);
		if(data->view_id > 0)
			WBUFW(buf,17+n*22)=data->view_id;
		else
			WBUFW(buf,17+n*22)=sd->status.cart[index].nameid;
		WBUFB(buf,19+n*22)=sd->status.cart[index].identify;
		WBUFB(buf,20+n*22)=sd->status.cart[index].attribute;
		WBUFB(buf,21+n*22)=sd->status.cart[index].refine;
		clif_addcards(WBUFP(buf, 22+n*22), &sd->status.cart[index]);
		n++;
	}
	if(n > 0){
		WBUFW(buf,0)=0x136;
		WBUFW(buf,2)=8+n*22;
		WBUFL(buf,4)=id;
		WFIFOSET(fd,WFIFOW(fd,2));
	}
	return n;
}

/*==========================================
 * 露店アイテム販売報告
 *------------------------------------------
*/
int clif_vendingreport(struct map_session_data *sd,int index,int amount)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x137]);
	WFIFOW(fd,0)=0x137;
	WFIFOW(fd,2)=index+2;
	WFIFOW(fd,4)=amount;
	WFIFOSET(fd,packet_len_table[0x137]);

	return 0;
}
/*==========================================
 * パーティ作成完了
 *------------------------------------------
 */
int clif_party_created(struct map_session_data *sd,int flag)
{
	int fd;

	// printf("clif_party_message(%s, %d, %s)\n", p->name, account_id, mes);

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xfa]);
	WFIFOW(fd,0)=0xfa;
	WFIFOB(fd,2)=flag;
	WFIFOSET(fd,packet_len_table[0xfa]);
	return 0;
}

int clif_party_main_info(struct party *p, int fd)
{
	struct map_session_data *sd;
	int i;
	unsigned char buf[96];
	
	for (i=0; i<MAX_PARTY && !p->member[i].leader; i++);
	if (i >= MAX_PARTY) return 0; //Should never happen...
	sd = p->member[i].sd;
	WBUFW(buf,0)=0x1e9;
	WBUFL(buf,2)= p->member[i].account_id;
	WBUFL(buf,6)= 0; //We don't know yet what this long is about.
	WBUFW(buf,10)=sd?sd->bl.x:0;
	WBUFW(buf,12)=sd?sd->bl.y:0;
	WBUFB(buf,14)=(p->member[i].online)?0:1;	//This byte is also unconfirmed...
	memcpy(WBUFP(buf,15), p->name, NAME_LENGTH);
	memcpy(WBUFP(buf,39), p->member[i].name, NAME_LENGTH);
	memcpy(WBUFP(buf,63), mapindex_id2name(p->member[i].map), MAP_NAME_LENGTH);
	WBUFB(buf,79) = (p->item&1)?1:0;
	WBUFB(buf,80) = (p->item&2)?1:0;
	if(fd>=0){
		WFIFOHEAD(fd,packet_len_table[0x1e9]);
		memcpy(WFIFOP(fd,0),buf,packet_len_table[0x1e9]);
		WFIFOSET(fd,packet_len_table[0x1e9]);
		return 1;
	}
	if (!sd) {
		for (i=0; i<MAX_PARTY && !p->member[i].sd; i++)
		if (i >= MAX_PARTY) return 0; //Should never happen...
		sd=p->member[i].sd;
	}
	clif_send(buf,packet_len_table[0x1e9],&sd->bl,PARTY);
	return 1;
}

int clif_party_join_info(struct party *p, struct map_session_data *sd)
{
	unsigned char buf[96];
	WBUFW(buf,0)=0x1e9;
	WBUFL(buf,2)= sd->status.account_id;
	WBUFL(buf,6)= 0; //Apparently setting this to 1 makes you adoptable.
	WBUFW(buf,10)=sd->bl.x;
	WBUFW(buf,12)=sd->bl.y;
	WBUFB(buf,14)=0; //Unconfirmed byte.
	memcpy(WBUFP(buf,15), p->name, NAME_LENGTH);
	memcpy(WBUFP(buf,39), sd->status.name, NAME_LENGTH);
	memcpy(WBUFP(buf,63), mapindex_id2name(sd->mapindex), MAP_NAME_LENGTH);
	WBUFB(buf,79) = (p->item&1)?1:0;
	WBUFB(buf,80) = (p->item&2)?1:0;
	clif_send(buf,packet_len_table[0x1e9],&sd->bl,PARTY_WOS);
	return 1;
}


/*==========================================
 * パーティ情報送信
 *------------------------------------------
 */
int clif_party_info(struct party *p,int fd)
{
	unsigned char buf[1024];
	int i,c;
	struct map_session_data *sd=NULL;

	nullpo_retr(0, p);

	WBUFW(buf,0)=0xfb;
	memcpy(WBUFP(buf,4),p->name,NAME_LENGTH);
	for(i=c=0;i<MAX_PARTY;i++){
		struct party_member *m=&p->member[i];
		if(m->account_id>0){
			if(sd==NULL) sd=m->sd;
			WBUFL(buf,28+c*46)=m->account_id;
			memcpy(WBUFP(buf,28+c*46+ 4),m->name,NAME_LENGTH);
			memcpy(WBUFP(buf,28+c*46+28),mapindex_id2name(m->map),MAP_NAME_LENGTH);
			WBUFB(buf,28+c*46+44)=(m->leader)?0:1;
			WBUFB(buf,28+c*46+45)=(m->online)?0:1;
			c++;
		}
	}
	WBUFW(buf,2)=28+c*46;
	if(fd>=0){	// fdが設定されてるならそれに送る
		WFIFOHEAD(fd, 28+c*46);
		memcpy(WFIFOP(fd,0),buf,WBUFW(buf,2));
		WFIFOSET(fd,WFIFOW(fd,2));
		return 9;
	}
	if(sd!=NULL)
		clif_send(buf,WBUFW(buf,2),&sd->bl,PARTY);
	return 0;
}
/*==========================================
 * パーティ勧誘
 *------------------------------------------
 */
int clif_party_invite(struct map_session_data *sd,struct map_session_data *tsd)
{
	int fd;
	struct party *p;

	nullpo_retr(0, sd);
	nullpo_retr(0, tsd);

	fd=tsd->fd;

	if( (p=party_search(sd->status.party_id))==NULL )
		return 0;

	WFIFOHEAD(fd,packet_len_table[0xfe]);
	WFIFOW(fd,0)=0xfe;
	WFIFOL(fd,2)=sd->status.account_id;
	memcpy(WFIFOP(fd,6),p->name,NAME_LENGTH);
	WFIFOSET(fd,packet_len_table[0xfe]);
	return 0;
}

/*==========================================
 * パーティ勧誘結果
 *------------------------------------------
 */
int clif_party_inviteack(struct map_session_data *sd,char *nick,int flag)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xfd]);
	WFIFOW(fd,0)=0xfd;
	memcpy(WFIFOP(fd,2),nick,NAME_LENGTH);
	WFIFOB(fd,26)=flag;
	WFIFOSET(fd,packet_len_table[0xfd]);
	return 0;
}

/*==========================================
 * パーティ設定送信
 * flag & 0x001=exp変更ミス
 *        0x010=item変更ミス
 *        0x100=一人にのみ送信
 *------------------------------------------
 */
int clif_party_option(struct party *p,struct map_session_data *sd,int flag)
{
	unsigned char buf[16];

	nullpo_retr(0, p);

//	if(battle_config.etc_log)
//		printf("clif_party_option: %d %d %d\n",p->exp,p->item,flag);
	if(sd==NULL && flag==0){
		int i;
		for(i=0;i<MAX_PARTY;i++)
			if((sd=map_id2sd(p->member[i].account_id))!=NULL)
				break;
	}
	if(sd==NULL)
		return 0;
	WBUFW(buf,0)=0x101;
	WBUFW(buf,2)=((flag&0x01)?2:p->exp);
	WBUFW(buf,4)=0; //NOTE: We don't know yet what this is for, it is NOT for item share rules, though. [Skotlex]
	if(flag==0)
		clif_send(buf,packet_len_table[0x101],&sd->bl,PARTY);
	else {
		WFIFOHEAD(sd->fd,packet_len_table[0x101]);
		memcpy(WFIFOP(sd->fd,0),buf,packet_len_table[0x101]);
		WFIFOSET(sd->fd,packet_len_table[0x101]);
	}
	return 0;
}
/*==========================================
 * パーティ脱退（脱退前に呼ぶこと）
 *------------------------------------------
 */
int clif_party_leaved(struct party *p,struct map_session_data *sd,int account_id,char *name,int flag)
{
	unsigned char buf[64];
	int i;

	nullpo_retr(0, p);

	WBUFW(buf,0)=0x105;
	WBUFL(buf,2)=account_id;
	memcpy(WBUFP(buf,6),name,NAME_LENGTH);
	WBUFB(buf,30)=flag&0x0f;

	if((flag&0xf0)==0){
		if(sd==NULL)
			for(i=0;i<MAX_PARTY;i++)
				if((sd=p->member[i].sd)!=NULL)
					break;
		if (sd!=NULL)
			clif_send(buf,packet_len_table[0x105],&sd->bl,PARTY);
	} else if (sd!=NULL) {
		WFIFOHEAD(sd->fd,packet_len_table[0x105]);
		memcpy(WFIFOP(sd->fd,0),buf,packet_len_table[0x105]);
		WFIFOSET(sd->fd,packet_len_table[0x105]);
	}
	return 0;
}
/*==========================================
 * パーティメッセージ送信
 *------------------------------------------
 */
int clif_party_message(struct party *p,int account_id,char *mes,int len)
{
	struct map_session_data *sd;
	int i;

	nullpo_retr(0, p);

	for(i=0;i<MAX_PARTY;i++){
		if((sd=p->member[i].sd)!=NULL)
			break;
	}
	if(sd!=NULL){
		unsigned char buf[1024];
		WBUFW(buf,0)=0x109;
		WBUFW(buf,2)=len+8;
		WBUFL(buf,4)=account_id;
		memcpy(WBUFP(buf,8),mes,len);
		clif_send(buf,len+8,&sd->bl,PARTY);
	}
	return 0;
}
/*==========================================
 * パーティ座標通知
 *------------------------------------------
 */
int clif_party_xy(struct map_session_data *sd)
{
	unsigned char buf[16];

	nullpo_retr(0, sd);

	WBUFW(buf,0)=0x107;
	WBUFL(buf,2)=sd->status.account_id;
	WBUFW(buf,6)=sd->bl.x;
	WBUFW(buf,8)=sd->bl.y;
	clif_send(buf,packet_len_table[0x107],&sd->bl,PARTY_SAMEMAP_WOS);
	
	return 0;
}

/*==========================================
 * Sends x/y dot to a single fd. [Skotlex]
 *------------------------------------------
 */

int clif_party_xy_single(int fd, struct map_session_data *sd)
{
	WFIFOHEAD(fd,packet_len_table[0x107]);
	WFIFOW(fd,0)=0x107;
	WFIFOL(fd,2)=sd->status.account_id;
	WFIFOW(fd,6)=sd->bl.x;
	WFIFOW(fd,8)=sd->bl.y;
	WFIFOSET(fd,packet_len_table[0x107]);
	return 0;
}


/*==========================================
 * パーティHP通知
 *------------------------------------------
 */
int clif_party_hp(struct map_session_data *sd)
{
	unsigned char buf[16];

	nullpo_retr(0, sd);

	WBUFW(buf,0)=0x106;
	WBUFL(buf,2)=sd->status.account_id;
	WBUFW(buf,6)=(sd->status.hp > SHRT_MAX)?SHRT_MAX:sd->status.hp;
	WBUFW(buf,8)=(sd->status.max_hp > SHRT_MAX)?SHRT_MAX:sd->status.max_hp;
	clif_send(buf,packet_len_table[0x106],&sd->bl,PARTY_AREA_WOS);
	return 0;
}

/*==========================================
 * Sends HP bar to a single fd. [Skotlex]
 *------------------------------------------
 */
static void clif_hpmeter_single(int fd, struct map_session_data *sd)
{
	WFIFOHEAD(fd,packet_len_table[0x106]);
	WFIFOW(fd,0) = 0x106;
	WFIFOL(fd,2) = sd->status.account_id;
	WFIFOW(fd,6) = (sd->status.hp > 0x7fff) ? 0x7fff : sd->status.hp;
	WFIFOW(fd,8) = (sd->status.max_hp > 0x7fff) ? 0x7fff : sd->status.max_hp;
	WFIFOSET (fd, packet_len_table[0x106]);
}

/*==========================================
 * GMへ場所とHP通知
 *------------------------------------------
 */
int clif_hpmeter(struct map_session_data *sd)
{
	struct map_session_data *sd2;
	unsigned char buf[16];
	int i, x0, y0, x1, y1;
	int level;

	nullpo_retr(0, sd);

	x0 = sd->bl.x - AREA_SIZE;
	y0 = sd->bl.y - AREA_SIZE;
	x1 = sd->bl.x + AREA_SIZE;
	y1 = sd->bl.y + AREA_SIZE;

	WBUFW(buf,0) = 0x106;
	WBUFL(buf,2) = sd->status.account_id;
	WBUFW(buf,6) = (sd->status.hp > 0x7fff) ? 0x7fff : sd->status.hp;
	WBUFW(buf,8) = (sd->status.max_hp > 0x7fff) ? 0x7fff : sd->status.max_hp;
	for (i = 0; i < fd_max; i++) {
		if (session[i] && (sd2 = (struct map_session_data*)session[i]->session_data) &&  sd != sd2 && sd2->state.auth) {
			if (sd2->bl.m != sd->bl.m || 
				sd2->bl.x < x0 || sd2->bl.y < y0 ||
				sd2->bl.x > x1 || sd2->bl.y > y1 ||
				(level = pc_isGM(sd2)) < battle_config.disp_hpmeter ||
				level < pc_isGM(sd))
				continue;
			WFIFOHEAD (i, packet_len_table[0x106]);
			memcpy (WFIFOP(i,0), buf, packet_len_table[0x106]);
			WFIFOSET (i, packet_len_table[0x106]);
		}
	}

	return 0;
}

/*==========================================
 * パーティ場所移動（未使用）
 *------------------------------------------
 */
int clif_party_move(struct party *p,struct map_session_data *sd,int online)
{
	unsigned char buf[128];

	nullpo_retr(0, sd);
	nullpo_retr(0, p);

	WBUFW(buf, 0)=0x104;
	WBUFL(buf, 2)=sd->status.account_id;
	WBUFL(buf, 6)=0;
	WBUFW(buf,10)=sd->bl.x;
	WBUFW(buf,12)=sd->bl.y;
	WBUFB(buf,14)=!online;
	memcpy(WBUFP(buf,15),p->name, NAME_LENGTH);
	memcpy(WBUFP(buf,39),sd->status.name, NAME_LENGTH);
	memcpy(WBUFP(buf,63),map[sd->bl.m].name, MAP_NAME_LENGTH);
	clif_send(buf,packet_len_table[0x104],&sd->bl,PARTY);
	return 0;
}
/*==========================================
 * 攻撃するために移動が必要
 *------------------------------------------
 */
int clif_movetoattack(struct map_session_data *sd,struct block_list *bl)
{
	int fd;

	nullpo_retr(0, sd);
	nullpo_retr(0, bl);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x139]);
	WFIFOW(fd, 0)=0x139;
	WFIFOL(fd, 2)=bl->id;
	WFIFOW(fd, 6)=bl->x;
	WFIFOW(fd, 8)=bl->y;
	WFIFOW(fd,10)=sd->bl.x;
	WFIFOW(fd,12)=sd->bl.y;
	WFIFOW(fd,14)=sd->attackrange;
	WFIFOSET(fd,packet_len_table[0x139]);
	return 0;
}
/*==========================================
 * 製造エフェクト
 *------------------------------------------
 */
int clif_produceeffect(struct map_session_data *sd,int flag,int nameid)
{
	int view,fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	// 名前の登録と送信を先にしておく
	if( map_charid2nick(sd->status.char_id)==NULL )
		map_addchariddb(sd->status.char_id,sd->status.name);
	clif_solved_charname(sd,sd->status.char_id);

	WFIFOHEAD(fd,packet_len_table[0x18f]);
	WFIFOW(fd, 0)=0x18f;
	WFIFOW(fd, 2)=flag;
	if((view = itemdb_viewid(nameid)) > 0)
		WFIFOW(fd, 4)=view;
	else
		WFIFOW(fd, 4)=nameid;
	WFIFOSET(fd,packet_len_table[0x18f]);
	return 0;
}

// pet
int clif_catch_process(struct map_session_data *sd)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x19e]);
	WFIFOW(fd,0)=0x19e;
	WFIFOSET(fd,packet_len_table[0x19e]);
	sd->menuskill_id = SA_TAMINGMONSTER;
	if (sd->ud.skillid == SA_TAMINGMONSTER)
		sd->menuskill_lv = 0;	//Free catch
	else
		sd->menuskill_lv = sd->itemid;	//Consume catch
	return 0;
}

int clif_pet_rulet(struct map_session_data *sd,int data)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x1a0]);
	WFIFOW(fd,0)=0x1a0;
	WFIFOB(fd,2)=data;
	WFIFOSET(fd,packet_len_table[0x1a0]);

	return 0;
}

/*==========================================
 * pet卵リスト作成
 *------------------------------------------
 */
int clif_sendegg(struct map_session_data *sd)
{
	//R 01a6 <len>.w <index>.w*
	int i,n=0,fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	if (battle_config.pet_no_gvg && map_flag_gvg(sd->bl.m))
	{	//Disable pet hatching in GvG grounds during Guild Wars [Skotlex]
		clif_displaymessage(fd, "Pets are not allowed in Guild Wars.");
		return 0;
	}
	WFIFOHEAD(fd, MAX_INVENTORY * 2 + 4);
	WFIFOW(fd,0)=0x1a6;
	if(sd->status.pet_id <= 0) {
		for(i=0,n=0;i<MAX_INVENTORY;i++){
			if(sd->status.inventory[i].nameid<=0 || sd->inventory_data[i] == NULL ||
			   sd->inventory_data[i]->type!=7 ||
			   sd->status.inventory[i].amount<=0)
				continue;
			WFIFOW(fd,n*2+4)=i+2;
			n++;
		}
	}
	WFIFOW(fd,2)=4+n*2;
	WFIFOSET(fd,WFIFOW(fd,2));

	sd->menuskill_id = SA_TAMINGMONSTER;
	sd->menuskill_lv = -1;
	return 0;
}

int clif_send_petdata(struct map_session_data *sd,int type,int param)
{
	int fd;

	nullpo_retr(0, sd);
	nullpo_retr(0, sd->pd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x1a4]);
	WFIFOW(fd,0)=0x1a4;
	WFIFOB(fd,2)=type;
	WFIFOL(fd,3)=sd->pd->bl.id;
	WFIFOL(fd,7)=param;
	WFIFOSET(fd,packet_len_table[0x1a4]);

	return 0;
}

int clif_send_petstatus(struct map_session_data *sd)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x1a2]);
	WFIFOW(fd,0)=0x1a2;
	memcpy(WFIFOP(fd,2),sd->pet.name,NAME_LENGTH);
	WFIFOB(fd,26)=(battle_config.pet_rename == 1)? 0:sd->pet.rename_flag;
	WFIFOW(fd,27)=sd->pet.level;
	WFIFOW(fd,29)=sd->pet.hungry;
	WFIFOW(fd,31)=sd->pet.intimate;
	WFIFOW(fd,33)=sd->pet.equip;
	WFIFOSET(fd,packet_len_table[0x1a2]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_pet_emotion(struct pet_data *pd,int param)
{
	unsigned char buf[16];
	struct map_session_data *sd;

	nullpo_retr(0, pd);
	nullpo_retr(0, sd = pd->msd);

	memset(buf,0,packet_len_table[0x1aa]);

	WBUFW(buf,0)=0x1aa;
	WBUFL(buf,2)=pd->bl.id;
	if(param >= 100 && sd->petDB->talk_convert_class) {
		if(sd->petDB->talk_convert_class < 0)
			return 0;
		else if(sd->petDB->talk_convert_class > 0) {
			param -= (pd->class_ - 100)*100;
			param += (sd->petDB->talk_convert_class - 100)*100;
		}
	}
	WBUFL(buf,6)=param;

	clif_send(buf,packet_len_table[0x1aa],&pd->bl,AREA);

	return 0;
}

int clif_pet_performance(struct block_list *bl,int param)
{
	unsigned char buf[16];

	nullpo_retr(0, bl);

	memset(buf,0,packet_len_table[0x1a4]);

	WBUFW(buf,0)=0x1a4;
	WBUFB(buf,2)=4;
	WBUFL(buf,3)=bl->id;
	WBUFL(buf,7)=param;

	clif_send(buf,packet_len_table[0x1a4],bl,AREA);

	return 0;
}

int clif_pet_equip(struct pet_data *pd,int nameid)
{
	unsigned char buf[16];

	nullpo_retr(0, pd);

	memset(buf,0,packet_len_table[0x1a4]);

	WBUFW(buf,0)=0x1a4;
	WBUFB(buf,2)=3;
	WBUFL(buf,3)=pd->bl.id;
	WBUFL(buf,7)=pd->vd.shield;
	clif_send(buf,packet_len_table[0x1a4],&pd->bl,AREA);

	return 0;
}

int clif_pet_food(struct map_session_data *sd,int foodid,int fail)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x1a3]);
	WFIFOW(fd,0)=0x1a3;
	WFIFOB(fd,2)=fail;
	WFIFOW(fd,3)=foodid;
	WFIFOSET(fd,packet_len_table[0x1a3]);

	return 0;
}

/*==========================================
 * オートスペル リスト送信
 *------------------------------------------
 */
int clif_autospell(struct map_session_data *sd,int skilllv)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x1cd]);
	WFIFOW(fd, 0)=0x1cd;

	if(skilllv>0 && pc_checkskill(sd,MG_NAPALMBEAT)>0)
		WFIFOL(fd,2)= MG_NAPALMBEAT;
	else
		WFIFOL(fd,2)= 0x00000000;
	if(skilllv>1 && pc_checkskill(sd,MG_COLDBOLT)>0)
		WFIFOL(fd,6)= MG_COLDBOLT;
	else
		WFIFOL(fd,6)= 0x00000000;
	if(skilllv>1 && pc_checkskill(sd,MG_FIREBOLT)>0)
		WFIFOL(fd,10)= MG_FIREBOLT;
	else
		WFIFOL(fd,10)= 0x00000000;
	if(skilllv>1 && pc_checkskill(sd,MG_LIGHTNINGBOLT)>0)
		WFIFOL(fd,14)= MG_LIGHTNINGBOLT;
	else
		WFIFOL(fd,14)= 0x00000000;
	if(skilllv>4 && pc_checkskill(sd,MG_SOULSTRIKE)>0)
		WFIFOL(fd,18)= MG_SOULSTRIKE;
	else
		WFIFOL(fd,18)= 0x00000000;
	if(skilllv>7 && pc_checkskill(sd,MG_FIREBALL)>0)
		WFIFOL(fd,22)= MG_FIREBALL;
	else
		WFIFOL(fd,22)= 0x00000000;
	if(skilllv>9 && pc_checkskill(sd,MG_FROSTDIVER)>0)
		WFIFOL(fd,26)= MG_FROSTDIVER;
	else
		WFIFOL(fd,26)= 0x00000000;

	WFIFOSET(fd,packet_len_table[0x1cd]);
	sd->menuskill_id = SA_AUTOSPELL;
	sd->menuskill_lv = skilllv;
	
	return 0;
}

/*==========================================
 * ディボーションの青い糸
 *------------------------------------------
 */
int clif_devotion(struct map_session_data *sd)
{
	unsigned char buf[56];
	int i,n;

	nullpo_retr(0, sd);

	WBUFW(buf,0)=0x1cf;
	WBUFL(buf,2)=sd->bl.id;
	for(i=0,n=0;i<5;i++) {
		if (!sd->devotion[i])
			continue;
		WBUFL(buf,6+4*n)=sd->devotion[i];
		n++;
	}
	for(;n<5;n++)
		WBUFL(buf,6+4*n)=0;
		
	WBUFB(buf,26)=8;
	WBUFB(buf,27)=0;

	clif_send(buf,packet_len_table[0x1cf],&sd->bl,AREA);
	return 0;
}

int clif_marionette(struct block_list *src, struct block_list *target)
{
	unsigned char buf[56];
	int n;

	WBUFW(buf,0)=0x1cf;
	WBUFL(buf,2)=src->id;
	for(n=0;n<5;n++)
		WBUFL(buf,6+4*n)=0;
	if (target) //The target goes on the second slot.
		WBUFL(buf,6+4) = target->id;
	WBUFB(buf,26)=8;
	WBUFB(buf,27)=0;

	clif_send(buf,packet_len_table[0x1cf],src,AREA);
	return 0;
}

/*==========================================
 * 氣球
 *------------------------------------------
 */
int clif_spiritball(struct map_session_data *sd)
{
	unsigned char buf[16];

	nullpo_retr(0, sd);

	WBUFW(buf,0)=0x1d0;
	WBUFL(buf,2)=sd->bl.id;
	WBUFW(buf,6)=sd->spiritball;
	clif_send(buf,packet_len_table[0x1d0],&sd->bl,AREA);
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_combo_delay(struct block_list *bl,int wait)
{
	unsigned char buf[32];

	nullpo_retr(0, bl);

	WBUFW(buf,0)=0x1d2;
	WBUFL(buf,2)=bl->id;
	WBUFL(buf,6)=wait;
	clif_send(buf,packet_len_table[0x1d2],bl,AREA);

	return 0;
}
/*==========================================
 *白刃取り
 *------------------------------------------
 */
int clif_bladestop(struct block_list *src,struct block_list *dst,
	int _bool)
{
	unsigned char buf[32];

	nullpo_retr(0, src);
	nullpo_retr(0, dst);

	WBUFW(buf,0)=0x1d1;
	WBUFL(buf,2)=src->id;
	WBUFL(buf,6)=dst->id;
	WBUFL(buf,10)=_bool;

	clif_send(buf,packet_len_table[0x1d1],src,AREA);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_changemapcell(int m,int x,int y,int cell_type,int type)
{
	struct block_list bl;
	unsigned char buf[32];

	bl.type = BL_NUL;
	bl.m = m;
	bl.x = x;
	bl.y = y;
	WBUFW(buf,0) = 0x192;
	WBUFW(buf,2) = x;
	WBUFW(buf,4) = y;
	WBUFW(buf,6) = cell_type;
	memcpy(WBUFP(buf,8),map[m].name,MAP_NAME_LENGTH);
	if(!type)
		clif_send(buf,packet_len_table[0x192],&bl,AREA);
	else
		clif_send(buf,packet_len_table[0x192],&bl,ALL_SAMEMAP);

	return 0;
}

/*==========================================
 * MVPエフェクト
 *------------------------------------------
 */
int clif_mvp_effect(struct map_session_data *sd)
{
	unsigned char buf[16];

	nullpo_retr(0, sd);

	WBUFW(buf,0)=0x10c;
	WBUFL(buf,2)=sd->bl.id;
	clif_send(buf,packet_len_table[0x10c],&sd->bl,AREA);
	return 0;
}
/*==========================================
 * MVPアイテム所得
 *------------------------------------------
 */
int clif_mvp_item(struct map_session_data *sd,int nameid)
{
	int view,fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x10a]);
	WFIFOW(fd,0)=0x10a;
	if((view = itemdb_viewid(nameid)) > 0)
		WFIFOW(fd,2)=view;
	else
		WFIFOW(fd,2)=nameid;
	WFIFOSET(fd,packet_len_table[0x10a]);
	return 0;
}
/*==========================================
 * MVP経験値所得
 *------------------------------------------
 */
int clif_mvp_exp(struct map_session_data *sd,int exp)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x10b]);
	WFIFOW(fd,0)=0x10b;
	WFIFOL(fd,2)=exp;
	WFIFOSET(fd,packet_len_table[0x10b]);
	return 0;
}

/*==========================================
 * ギルド作成可否通知
 *------------------------------------------
 */
int clif_guild_created(struct map_session_data *sd,int flag)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x167]);
	WFIFOW(fd,0)=0x167;
	WFIFOB(fd,2)=flag;
	WFIFOSET(fd,packet_len_table[0x167]);
	return 0;
}
/*==========================================
 * ギルド所属通知
 *------------------------------------------
 */
int clif_guild_belonginfo(struct map_session_data *sd,struct guild *g)
{
	int ps,fd;

	nullpo_retr(0, sd);
	nullpo_retr(0, g);

	fd=sd->fd;
	ps=guild_getposition(sd,g);

	WFIFOHEAD(fd,packet_len_table[0x16c]);
	memset(WFIFOP(fd,0),0,packet_len_table[0x16c]);
	WFIFOW(fd,0)=0x16c;
	WFIFOL(fd,2)=g->guild_id;
	WFIFOL(fd,6)=g->emblem_id;
	WFIFOL(fd,10)=g->position[ps].mode;
	memcpy(WFIFOP(fd,19),g->name,NAME_LENGTH);
	WFIFOSET(fd,packet_len_table[0x16c]);
	return 0;
}
/*==========================================
 * ギルドメンバログイン通知
 *------------------------------------------
 */
int clif_guild_memberlogin_notice(struct guild *g,int idx,int flag)
{
	unsigned char buf[64];

	nullpo_retr(0, g);

	// printf("clif_guild_message(%s, %d, %s)\n", g->name, account_id, mes);

	WBUFW(buf, 0)=0x16d;
	WBUFL(buf, 2)=g->member[idx].account_id;
	WBUFL(buf, 6)=g->member[idx].char_id;
	WBUFL(buf,10)=flag;
	if(g->member[idx].sd==NULL){
		struct map_session_data *sd=guild_getavailablesd(g);
		if(sd!=NULL)
			clif_send(buf,packet_len_table[0x16d],&sd->bl,GUILD);
	}else
		clif_send(buf,packet_len_table[0x16d],&g->member[idx].sd->bl,GUILD_WOS);
	return 0;
}

// Function `clif_guild_memberlogin_notice` sends info about
// logins and logouts of a guild member to the rest members.
// But at the 1st time (after a player login or map changing)
// the client won't show the message.
// So I suggest use this function for sending "first-time-info"
// to some player on entering the game or changing location. 
// At next time the client would always show the message.
// The function sends all the statuses in the single packet 
// to economize traffic. [LuzZza]
int clif_guild_send_onlineinfo(struct map_session_data *sd) {

	struct guild *g;
	char buf[14*128];
	int i, count=0, p_len;
	
	nullpo_retr(0, sd);

	p_len = packet_len_table[0x16d];

	if(!(g = guild_search(sd->status.guild_id)))
		return 0;
	
	for(i=0; i<g->max_member; i++) {

		if(g->member[i].account_id > 0 &&
			g->member[i].account_id != sd->status.account_id) {

			WBUFW(buf,count*p_len) = 0x16d;
			WBUFL(buf,count*p_len+2) = g->member[i].account_id;
			WBUFL(buf,count*p_len+6) = g->member[i].char_id;
			WBUFL(buf,count*p_len+10) = g->member[i].online;
			count++;
		}
	}
	
	clif_send(buf,p_len*count,&sd->bl,SELF);

	return 0;
}

/*==========================================
 * ギルドマスター通知(14dへの応答)
 *------------------------------------------
 */
int clif_guild_masterormember(struct map_session_data *sd)
{
	int type=0x57,fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	if(sd->state.gmaster_flag)
		type=0xd7;
	WFIFOHEAD(fd,packet_len_table[0x14e]);
	WFIFOW(fd,0)=0x14e;
	WFIFOL(fd,2)=type;
	WFIFOSET(fd,packet_len_table[0x14e]);
	return 0;
}
/*==========================================
 * Basic Info (Territories [Valaris])
 *------------------------------------------
 */
int clif_guild_basicinfo(struct map_session_data *sd)
{
	int fd,i,t=0;
	struct guild *g;
	struct guild_castle *gc=NULL;

	nullpo_retr(0, sd);

	fd=sd->fd;
	g=guild_search(sd->status.guild_id);
	if(g==NULL)
		return 0;

	WFIFOHEAD(fd,packet_len_table[0x1b6]);
	WFIFOW(fd, 0)=0x1b6;//0x150;
	WFIFOL(fd, 2)=g->guild_id;
	WFIFOL(fd, 6)=g->guild_lv;
	WFIFOL(fd,10)=g->connect_member;
	WFIFOL(fd,14)=g->max_member;
	WFIFOL(fd,18)=g->average_lv;
	WFIFOL(fd,22)=g->exp;
	WFIFOL(fd,26)=g->next_exp;
	WFIFOL(fd,30)=0;	// 上納
	WFIFOL(fd,34)=0;	// VW（性格の悪さ？：性向グラフ左右）
	WFIFOL(fd,38)=0;	// RF（正義の度合い？：性向グラフ上下）
	WFIFOL(fd,42)=0;	// 人数？
	memcpy(WFIFOP(fd,46),g->name, NAME_LENGTH);
	memcpy(WFIFOP(fd,70),g->master, NAME_LENGTH);

	for(i=0;i<MAX_GUILDCASTLE;i++){
		gc=guild_castle_search(i);
		if(!gc) continue;
			if(g->guild_id == gc->guild_id)	t++;
	}
	if (t>=0 && t<=MAX_GUILDCASTLE) //(0=None, 1..24 = N of Casles) [Lupus]
		strncpy((char*)WFIFOP(fd,94),msg_txt(300+t),20);
	else
		strncpy((char*)WFIFOP(fd,94),msg_txt(299),20);

	WFIFOSET(fd,packet_len_table[WFIFOW(fd,0)]);
	clif_guild_emblem(sd,g);	// Guild emblem vanish fix [Valaris]
	return 0;
}

/*==========================================
 * ギルド同盟/敵対情報
 *------------------------------------------
 */
int clif_guild_allianceinfo(struct map_session_data *sd)
{
	int fd,i,c;
	struct guild *g;

	nullpo_retr(0, sd);

	fd=sd->fd;
	g=guild_search(sd->status.guild_id);
	if(g==NULL)
		return 0;
	WFIFOHEAD(fd, MAX_GUILDALLIANCE * 32 + 4);
	WFIFOW(fd, 0)=0x14c;
	for(i=c=0;i<MAX_GUILDALLIANCE;i++){
		struct guild_alliance *a=&g->alliance[i];
		if(a->guild_id>0){
			WFIFOL(fd,c*32+4)=a->opposition;
			WFIFOL(fd,c*32+8)=a->guild_id;
			memcpy(WFIFOP(fd,c*32+12),a->name,NAME_LENGTH);
			c++;
		}
	}
	WFIFOW(fd, 2)=c*32+4;
	WFIFOSET(fd,WFIFOW(fd,2));
	return 0;
}

/*==========================================
 * ギルドメンバーリスト
 *------------------------------------------
 */
int clif_guild_memberlist(struct map_session_data *sd)
{
	int fd;
	int i,c;
	struct guild *g;

	nullpo_retr(0, sd);

	fd=sd->fd;
	if (!fd)
		return 0;
	g=guild_search(sd->status.guild_id);
	if(g==NULL)
		return 0;

	WFIFOHEAD(fd, g->max_member * 104 + 4);
	WFIFOW(fd, 0)=0x154;
	for(i=0,c=0;i<g->max_member;i++){
		struct guild_member *m=&g->member[i];
		if(m->account_id==0)
			continue;
		WFIFOL(fd,c*104+ 4)=m->account_id;
		WFIFOL(fd,c*104+ 8)=m->char_id;
		WFIFOW(fd,c*104+12)=m->hair;
		WFIFOW(fd,c*104+14)=m->hair_color;
		WFIFOW(fd,c*104+16)=m->gender;
		WFIFOW(fd,c*104+18)=m->class_;
		WFIFOW(fd,c*104+20)=m->lv;
		WFIFOL(fd,c*104+22)=m->exp;
		WFIFOL(fd,c*104+26)=m->online;
		WFIFOL(fd,c*104+30)=m->position;
		memset(WFIFOP(fd,c*104+34),0,50);	// メモ？
		memcpy(WFIFOP(fd,c*104+84),m->name,NAME_LENGTH);
		c++;
	}
	WFIFOW(fd, 2)=c*104+4;
	WFIFOSET(fd,WFIFOW(fd,2));
	return 0;
}
/*==========================================
 * ギルド役職名リスト
 *------------------------------------------
 */
int clif_guild_positionnamelist(struct map_session_data *sd)
{
	int i,fd;
	struct guild *g;

	nullpo_retr(0, sd);

	fd=sd->fd;
	g=guild_search(sd->status.guild_id);
	if(g==NULL)
		return 0;
	WFIFOHEAD(fd, MAX_GUILDPOSITION * 28 + 4);
	WFIFOW(fd, 0)=0x166;
	for(i=0;i<MAX_GUILDPOSITION;i++){
		WFIFOL(fd,i*28+4)=i;
		memcpy(WFIFOP(fd,i*28+8),g->position[i].name,NAME_LENGTH);
	}
	WFIFOW(fd,2)=i*28+4;
	WFIFOSET(fd,WFIFOW(fd,2));
	return 0;
}
/*==========================================
 * ギルド役職情報リスト
 *------------------------------------------
 */
int clif_guild_positioninfolist(struct map_session_data *sd)
{
	int i,fd;
	struct guild *g;

	nullpo_retr(0, sd);

	fd=sd->fd;
	g=guild_search(sd->status.guild_id);
	if(g==NULL)
		return 0;
	WFIFOHEAD(fd, MAX_GUILDPOSITION * 16 + 4);
	WFIFOW(fd, 0)=0x160;
	for(i=0;i<MAX_GUILDPOSITION;i++){
		struct guild_position *p=&g->position[i];
		WFIFOL(fd,i*16+ 4)=i;
		WFIFOL(fd,i*16+ 8)=p->mode;
		WFIFOL(fd,i*16+12)=i;
		WFIFOL(fd,i*16+16)=p->exp_mode;
	}
	WFIFOW(fd, 2)=i*16+4;
	WFIFOSET(fd,WFIFOW(fd,2));
	return 0;
}
/*==========================================
 * ギルド役職変更通知
 *------------------------------------------
 */
int clif_guild_positionchanged(struct guild *g,int idx)
{
	struct map_session_data *sd;
	unsigned char buf[128];

	nullpo_retr(0, g);

	WBUFW(buf, 0)=0x174;
	WBUFW(buf, 2)=44;
	WBUFL(buf, 4)=idx;
	WBUFL(buf, 8)=g->position[idx].mode;
	WBUFL(buf,12)=idx;
	WBUFL(buf,16)=g->position[idx].exp_mode;
	memcpy(WBUFP(buf,20),g->position[idx].name,NAME_LENGTH);
	if( (sd=guild_getavailablesd(g))!=NULL )
		clif_send(buf,WBUFW(buf,2),&sd->bl,GUILD);
	return 0;
}
/*==========================================
 * ギルドメンバ変更通知
 *------------------------------------------
 */
int clif_guild_memberpositionchanged(struct guild *g,int idx)
{
	struct map_session_data *sd;
	unsigned char buf[64];

	nullpo_retr(0, g);

	WBUFW(buf, 0)=0x156;
	WBUFW(buf, 2)=16;
	WBUFL(buf, 4)=g->member[idx].account_id;
	WBUFL(buf, 8)=g->member[idx].char_id;
	WBUFL(buf,12)=g->member[idx].position;
	if( (sd=guild_getavailablesd(g))!=NULL )
		clif_send(buf,WBUFW(buf,2),&sd->bl,GUILD);
	return 0;
}
/*==========================================
 * ギルドエンブレム送信
 *------------------------------------------
 */
int clif_guild_emblem(struct map_session_data *sd,struct guild *g)
{
	int fd;

	nullpo_retr(0, sd);
	nullpo_retr(0, g);

	fd=sd->fd;

	if(g->emblem_len<=0)
		return 0;
	WFIFOHEAD(fd,g->emblem_len+12);
	WFIFOW(fd,0)=0x152;
	WFIFOW(fd,2)=g->emblem_len+12;
	WFIFOL(fd,4)=g->guild_id;
	WFIFOL(fd,8)=g->emblem_id;
	memcpy(WFIFOP(fd,12),g->emblem_data,g->emblem_len);
	WFIFOSET(fd,WFIFOW(fd,2));
	return 0;
}
/*==========================================
 * ギルドスキル送信
 *------------------------------------------
 */
int clif_guild_skillinfo(struct map_session_data *sd)
{
	int fd;
	int i,id,c,up=1;
	struct guild *g;

	nullpo_retr(0, sd);

	fd=sd->fd;
	g=guild_search(sd->status.guild_id);
	if(g==NULL)
		return 0;
	WFIFOHEAD(fd, MAX_GUILDSKILL * 37 + 6);
	WFIFOW(fd,0)=0x0162;
	WFIFOW(fd,4)=g->skill_point;
	for(i=c=0;i<MAX_GUILDSKILL;i++){
		if(g->skill[i].id>0 && guild_check_skill_require(g,g->skill[i].id)){
			WFIFOW(fd,c*37+ 6) = id = g->skill[i].id;
			WFIFOW(fd,c*37+ 8) = skill_get_inf(id);
			WFIFOW(fd,c*37+10) = 0;
			WFIFOW(fd,c*37+12) = g->skill[i].lv;
			WFIFOW(fd,c*37+14) = skill_get_sp(id,g->skill[i].lv);
			WFIFOW(fd,c*37+16) = skill_get_range(id,g->skill[i].lv);
			memset(WFIFOP(fd,c*37+18),0,24);
			if(g->skill[i].lv < guild_skill_get_max(id) && (sd == g->member[0].sd))
				up = 1;
			else
				up = 0;
			WFIFOB(fd,c*37+42)= up;
			c++;
		}
	}
	WFIFOW(fd,2)=c*37+6;
	WFIFOSET(fd,WFIFOW(fd,2));
	return 0;
}
/*==========================================
 * ギルド告知送信
 *------------------------------------------
 */
int clif_guild_notice(struct map_session_data *sd,struct guild *g)
{
	int fd;

	nullpo_retr(0, sd);
	nullpo_retr(0, g);

	fd = sd->fd;

	if ( !session_isActive(fd) )  //null pointer right here [Kevin]
		return 0;
 
	if (fd <= 0)
		return 0;
	if(*g->mes1==0 && *g->mes2==0)
		return 0;
	WFIFOHEAD(fd,packet_len_table[0x16f]);
	WFIFOW(fd,0)=0x16f;
	memcpy(WFIFOP(fd,2),g->mes1,60);
	memcpy(WFIFOP(fd,62),g->mes2,120);
	WFIFOSET(fd,packet_len_table[0x16f]);
	return 0;
}

/*==========================================
 * ギルドメンバ勧誘
 *------------------------------------------
 */
int clif_guild_invite(struct map_session_data *sd,struct guild *g)
{
	int fd;

	nullpo_retr(0, sd);
	nullpo_retr(0, g);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x16a]);
	WFIFOW(fd,0)=0x16a;
	WFIFOL(fd,2)=g->guild_id;
	memcpy(WFIFOP(fd,6),g->name,NAME_LENGTH);
	WFIFOSET(fd,packet_len_table[0x16a]);
	return 0;
}
/*==========================================
 * ギルドメンバ勧誘結果
 *------------------------------------------
 */
int clif_guild_inviteack(struct map_session_data *sd,int flag)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x169]);
	WFIFOW(fd,0)=0x169;
	WFIFOB(fd,2)=flag;
	WFIFOSET(fd,packet_len_table[0x169]);
	return 0;
}
/*==========================================
 * ギルドメンバ脱退通知
 *------------------------------------------
 */
int clif_guild_leave(struct map_session_data *sd,const char *name,const char *mes)
{
	unsigned char buf[128];

	nullpo_retr(0, sd);

	WBUFW(buf, 0)=0x15a;
	memcpy(WBUFP(buf, 2),name,NAME_LENGTH);
	memcpy(WBUFP(buf,26),mes,40);
	clif_send(buf,packet_len_table[0x15a],&sd->bl,GUILD);
	return 0;
}
/*==========================================
 * ギルドメンバ追放通知
 *------------------------------------------
 */
int clif_guild_explusion(struct map_session_data *sd,const char *name,const char *mes,
	int account_id)
{
	unsigned char buf[128];

	nullpo_retr(0, sd);

	WBUFW(buf, 0)=0x15c;
	memcpy(WBUFP(buf, 2),name,NAME_LENGTH);
	memcpy(WBUFP(buf,26),mes,40);
	memcpy(WBUFP(buf,66),"dummy",NAME_LENGTH);
	clif_send(buf,packet_len_table[0x15c],&sd->bl,GUILD);
	return 0;
}
/*==========================================
 * ギルド追放メンバリスト
 *------------------------------------------
 */
int clif_guild_explusionlist(struct map_session_data *sd)
{
	int fd;
	int i,c;
	struct guild *g;

	nullpo_retr(0, sd);

	fd=sd->fd;
	g=guild_search(sd->status.guild_id);
	if(g==NULL)
		return 0;
	WFIFOHEAD(fd,MAX_GUILDEXPLUSION * 88 + 4);
	WFIFOW(fd,0)=0x163;
	for(i=c=0;i<MAX_GUILDEXPLUSION;i++){
		struct guild_explusion *e=&g->explusion[i];
		if(e->account_id>0){
			memcpy(WFIFOP(fd,c*88+ 4),e->name,NAME_LENGTH);
			memcpy(WFIFOP(fd,c*88+28),e->acc,24);
			memcpy(WFIFOP(fd,c*88+52),e->mes,44);
			c++;
		}
	}
	WFIFOW(fd,2)=c*88+4;
	WFIFOSET(fd,WFIFOW(fd,2));
	return 0;
}

/*==========================================
 * ギルド会話
 *------------------------------------------
 */
int clif_guild_message(struct guild *g,int account_id,const char *mes,int len)
{
	struct map_session_data *sd;
	unsigned char *buf;

	buf = (unsigned char*)aMallocA((len + 4)*sizeof(unsigned char));

	WBUFW(buf, 0) = 0x17f;
	WBUFW(buf, 2) = len + 4;
	memcpy(WBUFP(buf,4), mes, len);

	if ((sd = guild_getavailablesd(g)) != NULL)
		clif_send(buf, WBUFW(buf,2), &sd->bl, GUILD);

	if(buf) aFree(buf);

	return 0;
}
/*==========================================
 * ギルドスキル割り振り通知
 *------------------------------------------
 */
int clif_guild_skillup(struct map_session_data *sd,int skill_num,int lv)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,11);
	WFIFOW(fd,0) = 0x10e;
	WFIFOW(fd,2) = skill_num;
	WFIFOW(fd,4) = lv;
	WFIFOW(fd,6) = skill_get_sp(skill_num,lv);
	WFIFOW(fd,8) = skill_get_range(skill_num,lv);
	WFIFOB(fd,10) = 1;
	WFIFOSET(fd,11);
	return 0;
}
/*==========================================
 * ギルド同盟要請
 *------------------------------------------
 */
int clif_guild_reqalliance(struct map_session_data *sd,int account_id,const char *name)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x171]);
	WFIFOW(fd,0)=0x171;
	WFIFOL(fd,2)=account_id;
	memcpy(WFIFOP(fd,6),name,NAME_LENGTH);
	WFIFOSET(fd,packet_len_table[0x171]);
	return 0;
}
/*==========================================
 * ギルド同盟結果
 *------------------------------------------
 */
int clif_guild_allianceack(struct map_session_data *sd,int flag)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x173]);
	WFIFOW(fd,0)=0x173;
	WFIFOL(fd,2)=flag;
	WFIFOSET(fd,packet_len_table[0x173]);
	return 0;
}
/*==========================================
 * ギルド関係解消通知
 *------------------------------------------
 */
int clif_guild_delalliance(struct map_session_data *sd,int guild_id,int flag)
{
	int fd;

	nullpo_retr(0, sd);

	fd = sd->fd;
	if (fd <= 0)
		return 0;
	WFIFOHEAD(fd,packet_len_table[0x184]);
	WFIFOW(fd,0)=0x184;
	WFIFOL(fd,2)=guild_id;
	WFIFOL(fd,6)=flag;
	WFIFOSET(fd,packet_len_table[0x184]);
	return 0;
}
/*==========================================
 * ギルド敵対結果
 *------------------------------------------
 */
int clif_guild_oppositionack(struct map_session_data *sd,int flag)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x181]);
	WFIFOW(fd,0)=0x181;
	WFIFOB(fd,2)=flag;
	WFIFOSET(fd,packet_len_table[0x181]);
	return 0;
}
/*==========================================
 * ギルド関係追加
 *------------------------------------------
 */
/*int clif_guild_allianceadded(struct guild *g,int idx)
{
	unsigned char buf[64];
	WBUFW(fd,0)=0x185;
	WBUFL(fd,2)=g->alliance[idx].opposition;
	WBUFL(fd,6)=g->alliance[idx].guild_id;
	memcpy(WBUFP(fd,10),g->alliance[idx].name,NAME_LENGTH);
	clif_send(buf,packet_len_table[0x185],guild_getavailablesd(g),GUILD);
	return 0;
}*/

/*==========================================
 * ギルド解散通知
 *------------------------------------------
 */
int clif_guild_broken(struct map_session_data *sd,int flag)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x15e]);
	WFIFOW(fd,0)=0x15e;
	WFIFOL(fd,2)=flag;
	WFIFOSET(fd,packet_len_table[0x15e]);
	return 0;
}

/*==========================================
 * エモーション
 *------------------------------------------
 */
void clif_emotion(struct block_list *bl,int type)
{
	unsigned char buf[8];

	nullpo_retv(bl);

	WBUFW(buf,0)=0xc0;
	WBUFL(buf,2)=bl->id;
	WBUFB(buf,6)=type;
	clif_send(buf,packet_len_table[0xc0],bl,AREA);
}

/*==========================================
 * トーキーボックス
 *------------------------------------------
 */
void clif_talkiebox(struct block_list *bl,char* talkie)
{
	unsigned char buf[86];

	nullpo_retv(bl);

	WBUFW(buf,0)=0x191;
	WBUFL(buf,2)=bl->id;
	memcpy(WBUFP(buf,6),talkie,MESSAGE_SIZE);
	clif_send(buf,packet_len_table[0x191],bl,AREA);
}

/*==========================================
 * 結婚エフェクト
 *------------------------------------------
 */
void clif_wedding_effect(struct block_list *bl) {
	unsigned char buf[6];

	nullpo_retv(bl);

	WBUFW(buf,0) = 0x1ea;
	WBUFL(buf,2) = bl->id;
	clif_send(buf, packet_len_table[0x1ea], bl, AREA);
}
/*==========================================
 * あなたに逢いたい使用時名前叫び
 *------------------------------------------

void clif_callpartner(struct map_session_data *sd)
{
	unsigned char buf[26];
	char *p;

	nullpo_retv(sd);

	if(sd->status.partner_id){
		WBUFW(buf,0)=0x1e6;
		p = map_charid2nick(sd->status.partner_id);
		if(p){
			memcpy(WBUFP(buf,2),p,NAME_LENGTH);
		}else{
			map_reqchariddb(sd,sd->status.partner_id);
			chrif_searchcharid(sd->status.partner_id);
			WBUFB(buf,2) = 0;
		}
		clif_send(buf,packet_len_table[0x1e6],&sd->bl,AREA);
	}
	return;
}
*/
/*==========================================
 * Adopt baby [Celest]
 *------------------------------------------
 */
void clif_adopt_process(struct map_session_data *sd)
{
	int fd;
	nullpo_retv(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x1f8]);
	WFIFOW(fd,0)=0x1f8;
	WFIFOSET(fd,packet_len_table[0x1f8]);
}
/*==========================================
 * Marry [DracoRPG]
 *------------------------------------------
 */
void clif_marriage_process(struct map_session_data *sd)
{
	int fd;
	nullpo_retv(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x1e4]);
	WFIFOW(fd,0)=0x1e4;
	WFIFOSET(fd,packet_len_table[0x1e4]);
}


/*==========================================
 * Notice of divorce
 *------------------------------------------
 */
void clif_divorced(struct map_session_data *sd, char *name)
{
	int fd;
	nullpo_retv(sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x205]);
	WFIFOW(fd,0)=0x205;
	memcpy(WFIFOP(fd,2), name, NAME_LENGTH);
	WFIFOSET(fd, packet_len_table[0x205]);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_ReqAdopt(int fd, struct map_session_data *sd) {
	nullpo_retv(sd);

	WFIFOHEAD(fd,packet_len_table[0x1f6]);
	WFIFOW(fd,0)=0x1f6;
	WFIFOSET(fd, packet_len_table[0x1f6]);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_ReqMarriage(int fd, struct map_session_data *sd) {
	nullpo_retv(sd);

	WFIFOHEAD(fd,packet_len_table[0x1e2]);
	WFIFOW(fd,0)=0x1e2;
	WFIFOSET(fd, packet_len_table[0x1e2]);
}

/*==========================================
 * 座る
 *------------------------------------------
 */
void clif_sitting(struct map_session_data *sd)
{
	unsigned char buf[64];

	nullpo_retv(sd);

	WBUFW(buf, 0) = 0x8a;
	WBUFL(buf, 2) = sd->bl.id;
	WBUFB(buf,26) = 2;
	clif_send(buf, packet_len_table[0x8a], &sd->bl, AREA);
}

/*==========================================
 *
 *------------------------------------------
 */
int clif_disp_onlyself(struct map_session_data *sd, char *mes, int len)
{
	unsigned char *buf;

	nullpo_retr(0, sd);

	buf = (unsigned char*)aMallocA((len + 5)*sizeof(unsigned char));

	WBUFW(buf, 0) = 0x17f;
	WBUFW(buf, 2) = len + 5;
	memcpy(WBUFP(buf,4), mes, len);

	clif_send(buf, WBUFW(buf,2), &sd->bl, SELF);

	if(buf) aFree(buf);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */

int clif_GM_kickack(struct map_session_data *sd, int id)
{
	int fd;

	nullpo_retr(0, sd);

	fd = sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xcd]);
	WFIFOW(fd,0) = 0xcd;
	WFIFOL(fd,2) = id;
	WFIFOSET(fd, packet_len_table[0xcd]);
	return 0;
}

void clif_parse_QuitGame(int fd,struct map_session_data *sd);

int clif_GM_kick(struct map_session_data *sd,struct map_session_data *tsd,int type)
{
	nullpo_retr(0, tsd);

	if(type)
		clif_GM_kickack(sd,tsd->status.account_id);
	WFIFOHEAD(tsd->fd,packet_len_table[0x18b]);
	WFIFOW(tsd->fd,0) = 0x18b;
	WFIFOW(tsd->fd,2) = 0;
	WFIFOSET(tsd->fd,packet_len_table[0x18b]);

	if (tsd->fd)
	{
		ShowDebug("clif_GM_kick: Disconnecting session #%d\n", tsd->fd);
		clif_setwaitclose(tsd->fd);
	} else { //Player has no session attached, delete it right away. [Skotlex]
		map_quit(tsd);
	}

	return 0;
}

int clif_GM_silence(struct map_session_data *sd, struct map_session_data *tsd, int type)
{
	int fd;
	
	nullpo_retr(0, sd);
	nullpo_retr(0, tsd);

	fd = tsd->fd;
	if (fd <= 0)
		return 0;
	WFIFOHEAD(fd,packet_len_table[0x14b]);
	WFIFOW(fd,0) = 0x14b;
	WFIFOB(fd,2) = 0;
	memcpy(WFIFOP(fd,3), sd->status.name, NAME_LENGTH);
	WFIFOSET(fd, packet_len_table[0x14b]);

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */

int clif_timedout(struct map_session_data *sd)
{
	nullpo_retr(0, sd);

	ShowInfo("%sCharacter with Account ID '"CL_WHITE"%d"CL_RESET"' timed out.\n", (pc_isGM(sd))?"GM ":"", sd->bl.id);
	clif_authfail_fd(sd->fd,3); // Even if player is not on we still send anyway
	clif_setwaitclose(sd->fd); // Set session to EOF
	return 0;
}

/*==========================================
 * Wis拒否許可応答
 *------------------------------------------
 */
int clif_wisexin(struct map_session_data *sd,int type,int flag)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xd1]);
	WFIFOW(fd,0)=0xd1;
	WFIFOB(fd,2)=type;
	WFIFOB(fd,3)=flag;
	WFIFOSET(fd,packet_len_table[0xd1]);

	return 0;
}
/*==========================================
 * Wis全拒否許可応答
 *------------------------------------------
 */
int clif_wisall(struct map_session_data *sd,int type,int flag)
{
	int fd;

	nullpo_retr(0, sd);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0xd2]);
	WFIFOW(fd,0)=0xd2;
	WFIFOB(fd,2)=type;
	WFIFOB(fd,3)=flag;
	WFIFOSET(fd,packet_len_table[0xd2]);

	return 0;
}
/*==========================================
 * サウンドエフェクト
 *------------------------------------------
 */
void clif_soundeffect(struct map_session_data *sd,struct block_list *bl,char *name,int type)
{
	int fd;

	nullpo_retv(sd);
	nullpo_retv(bl);

	fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x1d3]);
	WFIFOW(fd,0)=0x1d3;
	memcpy(WFIFOP(fd,2),name,NAME_LENGTH);
	WFIFOB(fd,26)=type;
	WFIFOL(fd,27)=0;
	WFIFOL(fd,31)=bl->id;
	WFIFOSET(fd,packet_len_table[0x1d3]);

	return;
}

int clif_soundeffectall(struct block_list *bl, char *name, int type, int coverage)
{
	unsigned char buf[40];
	memset(buf, 0, packet_len_table[0x1d3]);

	if(coverage < 0 || coverage > 22){
		ShowError("clif_soundeffectall: undefined coverage.\n");
		return 0;
	}

	nullpo_retr(0, bl);

	WBUFW(buf,0)=0x1d3;
	memcpy(WBUFP(buf,2), name, NAME_LENGTH);
	WBUFB(buf,26)=type;
	WBUFL(buf,27)=0;
	WBUFL(buf,31)=bl->id;
	clif_send(buf, packet_len_table[0x1d3], bl, coverage);

	return 0;
}

// displaying special effects (npcs, weather, etc) [Valaris]
int clif_specialeffect(struct block_list *bl, int type, int flag)
{
	unsigned char buf[24];

	nullpo_retr(0, bl);

	memset(buf, 0, packet_len_table[0x1f3]);

	WBUFW(buf,0) = 0x1f3;
	WBUFL(buf,2) = bl->id;
	WBUFL(buf,6) = type;

	switch (flag) {
	case 4:
		clif_send(buf, packet_len_table[0x1f3], bl, AREA_WOS);
		break;
	case 3:
		clif_send(buf, packet_len_table[0x1f3], bl, ALL_CLIENT);
		break;
	case 2:
		clif_send(buf, packet_len_table[0x1f3], bl, ALL_SAMEMAP);
		break;
	case 1:
		clif_send(buf, packet_len_table[0x1f3], bl, SELF);
		break;
	default:
		clif_send(buf, packet_len_table[0x1f3], bl, AREA);
	}
	if (disguised(bl)) {
		WBUFL(buf,2) = -bl->id;
		clif_send(buf, packet_len_table[0x1f3], bl, SELF);
	}
	return 0;
}

// refresh the client's screen, getting rid of any effects
int clif_refresh(struct map_session_data *sd) {
	nullpo_retr(-1, sd);
	clif_changemap(sd,sd->mapindex,sd->bl.x,sd->bl.y);
	map_foreachinarea(clif_getareachar,sd->bl.m,sd->bl.x-AREA_SIZE,sd->bl.y-AREA_SIZE,sd->bl.x+AREA_SIZE,sd->bl.y+AREA_SIZE,BL_ALL,sd);
	return 0;
}

// updates the object's (bl) name on client
int clif_charnameack (int fd, struct block_list *bl)
{
	unsigned char buf[103];
	int cmd = 0x95;

	nullpo_retr(0, bl);

	WBUFW(buf,0) = cmd;
	WBUFL(buf,2) = bl->id;

	switch(bl->type) {
	case BL_PC:
		{
			struct map_session_data *ssd = (struct map_session_data *)bl;
			struct party *p = NULL;
			struct guild *g = NULL;
			
			//Requesting your own "shadow" name. [Skotlex]
			if (ssd->fd == fd && ssd->disguise)
				WBUFL(buf,2) = -bl->id; 

			if (strlen(ssd->fakename)>1) {
				memcpy(WBUFP(buf,6), ssd->fakename, NAME_LENGTH);
				break;
			}
			memcpy(WBUFP(buf,6), ssd->status.name, NAME_LENGTH);
			
			if (ssd->status.party_id > 0)
				p = party_search(ssd->status.party_id);

			if (ssd->status.guild_id > 0)
				g = guild_search(ssd->status.guild_id);

			if (p == NULL && g == NULL)
				break;
			
			WBUFW(buf, 0) = cmd = 0x195;
			if (p)
				memcpy(WBUFP(buf,30), p->name, NAME_LENGTH);
			else
				WBUFB(buf,30) = 0;
			
			if (g)
			{
				int i, ps = -1;
				for(i = 0; i < g->max_member; i++) {
					if (g->member[i].account_id == ssd->status.account_id &&
						g->member[i].char_id == ssd->status.char_id )
						{
							ps = g->member[i].position;
							break;
						}
					}
				if (ps >= 0 && ps < MAX_GUILDPOSITION)
				{
					memcpy(WBUFP(buf,54), g->name,NAME_LENGTH);
					memcpy(WBUFP(buf,78), g->position[ps].name, NAME_LENGTH);
				} else { //Assume no guild.
					WBUFB(buf,54) = 0;
					WBUFB(buf,78) = 0;
				}
			} else {
				WBUFB(buf,54) = 0;
				WBUFB(buf,78) = 0;
			}
		}
		break;
	//[blackhole89]
	case BL_HOMUNCULUS:
		memcpy(WBUFP(buf,6), ((struct homun_data*)bl)->name, NAME_LENGTH);
		break;
	case BL_PET:
		memcpy(WBUFP(buf,6), ((struct pet_data*)bl)->name, NAME_LENGTH);
		break;
	case BL_NPC:
		memcpy(WBUFP(buf,6), ((struct npc_data*)bl)->name, NAME_LENGTH);
		break;
	case BL_MOB:
		{
			struct mob_data *md = (struct mob_data *)bl;
			nullpo_retr(0, md);

			memcpy(WBUFP(buf,6), md->name, NAME_LENGTH);
			if (md->guardian_data && md->guardian_data->guild_id) {
				WBUFW(buf, 0) = cmd = 0x195;
				WBUFB(buf,30) = 0;
				memcpy(WBUFP(buf,54), md->guardian_data->guild_name, NAME_LENGTH);
				memcpy(WBUFP(buf,78), md->guardian_data->castle->castle_name, NAME_LENGTH);
			} else if (battle_config.show_mob_hp == 1) {
				char mobhp[50];
				WBUFW(buf, 0) = cmd = 0x195;
				sprintf(mobhp, "HP: %d/%d", md->hp, md->max_hp);
				//Even thought mobhp ain't a name, we send it as one so the client
				//can parse it. [Skotlex]
				memcpy(WBUFP(buf,30), mobhp, NAME_LENGTH);
				WBUFB(buf,54) = 0;
				memcpy(WBUFP(buf,78), mobhp, NAME_LENGTH);
			}
		}
		break;
	case BL_CHAT:	//FIXME: Clients DO request this... what should be done about it? The chat's title may not fit... [Skotlex]
//		memcpy(WBUFP(buf,6), (struct chat*)->title, NAME_LENGTH);
//		break;
		return 0;
	default:
		if (battle_config.error_log)
			ShowError("clif_parse_GetCharNameRequest : bad type %d(%d)\n", bl->type, bl->id);
		return 0;
	}

	// if no receipient specified just update nearby clients
	if (fd == 0)
		clif_send(buf, packet_len_table[cmd], bl, AREA);
	else {
		WFIFOHEAD(fd, packet_len_table[cmd]);
		memcpy(WFIFOP(fd, 0), buf, packet_len_table[cmd]);
		WFIFOSET(fd, packet_len_table[cmd]);
	}

	return 0;
}

//Used to update when a char leaves a party/guild. [Skotlex]
//Needed because when you send a 0x95 packet, the client will not remove the cached party/guild info that is not sent.
int clif_charnameupdate (struct map_session_data *ssd)
{
	unsigned char buf[103];
	int cmd = 0x195;
	struct party *p = NULL;
	struct guild *g = NULL;

	nullpo_retr(0, ssd);

	if (strlen(ssd->fakename)>1)
		return 0; //No need to update as the party/guild was not displayed anyway.

	WBUFW(buf,0) = cmd;
	WBUFL(buf,2) = ssd->bl.id;

	memcpy(WBUFP(buf,6), ssd->status.name, NAME_LENGTH);
			
	if (ssd->status.party_id > 0)
		p = party_search(ssd->status.party_id);

	if (ssd->status.guild_id > 0)
		g = guild_search(ssd->status.guild_id);

	if (p)
		memcpy(WBUFP(buf,30), p->name, NAME_LENGTH);
	else
		WBUFB(buf,30) = 0;
			
	if (g)
	{
		int i, ps = -1;
		for(i = 0; i < g->max_member; i++) {
			if (g->member[i].account_id == ssd->status.account_id &&
				g->member[i].char_id == ssd->status.char_id )
				{
					ps = g->member[i].position;
					break;
				}
			}
		if (ps >= 0 && ps < MAX_GUILDPOSITION)
		{
			memcpy(WBUFP(buf,54), g->name,NAME_LENGTH);
			memcpy(WBUFP(buf,78), g->position[ps].name, NAME_LENGTH);
		} else { //Assume no guild.
			WBUFB(buf,54) = 0;
			WBUFB(buf,78) = 0;
		}
	} else {
		WBUFB(buf,54) = 0;
		WBUFB(buf,78) = 0;
	}

	// Update nearby clients
	clif_send(buf, packet_len_table[cmd], &ssd->bl, AREA);
	return 0;
}

int clif_slide(struct block_list *bl, int x, int y){
	unsigned char buf[10];

	nullpo_retr(0, bl);

	WBUFW(buf, 0) = 0x01ff;
	WBUFL(buf, 2) = bl->id;
	WBUFW(buf, 6) = x;
	WBUFW(buf, 8) = y;

	clif_send(buf, packet_len_table[0x1ff], bl, AREA);
	return 0;
}

/*------------------------------------------
 * @me command by lordalfa, rewritten implementation by Skotlex
 *------------------------------------------
*/
int clif_disp_overhead(struct map_session_data *sd, char* mes)
{
	unsigned char buf[256]; //This should be more than sufficient, the theorical max is MESSAGE_SIZE+NAME_LENGTH + 8 (pads and extra inserted crap)
	int len_mes = strlen(mes)+1; //Account for \0

	if (len_mes + 8 >= 256) {
		if (battle_config.error_log)
			ShowError("clif_disp_overhead: Message too long (length %d)\n", len_mes);
		len_mes = 247; //Trunk it to avoid problems.
	}
	// send message to others
	WBUFW(buf,0) = 0x8d;
	WBUFW(buf,2) = len_mes + 8; // len of message + 8 (command+len+id)
	WBUFL(buf,4) = sd->bl.id;
	memcpy(WBUFP(buf,8), mes, len_mes);
	clif_send(buf, WBUFW(buf,2), &sd->bl, AREA_CHAT_WOC);

	// send back message to the speaker
	WBUFW(buf,0) = 0x8e;
	WBUFW(buf, 2) = len_mes + 4;
	memcpy(WBUFP(buf,4), mes, len_mes);  
	clif_send(buf, WBUFW(buf,2), &sd->bl, SELF);

	return 0;
}

/*==========================
 * Minimap fix [Kevin]
 * Remove dot from minimap 
 *--------------------------
*/
int clif_party_xy_remove(struct map_session_data *sd)
{
	unsigned char buf[16];
	nullpo_retr(0, sd);
	WBUFW(buf,0)=0x107;
	WBUFL(buf,2)=sd->status.account_id;
	WBUFW(buf,6)=-1;
	WBUFW(buf,8)=-1;
	clif_send(buf,packet_len_table[0x107],&sd->bl,PARTY_SAMEMAP_WOS);
	return 0;
}

/*==========================================
 * Info about Star Glaldiator save map [Komurka]
 *------------------------------------------
 */
void clif_feel_info(struct map_session_data *sd, int feel_level)
{
	int fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x20e]);
	WFIFOW(fd,0)=0x20e;
	memcpy(WFIFOP(fd,2),mapindex_id2name(sd->feel_map[feel_level].index), MAP_NAME_LENGTH);
	WFIFOL(fd,26)=sd->bl.id;
	WFIFOW(fd,30)=0x100+feel_level;
	WFIFOSET(fd, packet_len_table[0x20e]);
}

/*==========================================
 * Info about Star Glaldiator hate mob [Komurka]
 *------------------------------------------
 */
void clif_hate_mob(struct map_session_data *sd, int skilllv,int mob_id)
{
	int fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x20e]);
	WFIFOW(fd,0)=0x20e;
	if (pcdb_checkid(mob_id))
		strncpy(WFIFOP(fd,2),job_name(mob_id), NAME_LENGTH);
	else if (mobdb_checkid(mob_id))
		strncpy(WFIFOP(fd,2),mob_db(mob_id)->jname, NAME_LENGTH);
	else //Really shouldn't happen...
		memset(WFIFOP(fd,2), 0, NAME_LENGTH);
	WFIFOL(fd,26)=sd->bl.id;
	WFIFOW(fd,30)=0xa00+skilllv-1;
	WFIFOSET(fd, packet_len_table[0x20e]);
}

/*==========================================
 * Info about TaeKwon Do TK_MISSION mob [Skotlex]
 *------------------------------------------
 */
void clif_mission_mob(struct map_session_data *sd, unsigned short mob_id, unsigned short progress)
{
	int fd=sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x20e]);
	WFIFOW(fd,0)=0x20e;
	strncpy(WFIFOP(fd,2),mob_db(mob_id)->jname, NAME_LENGTH);
	WFIFOL(fd,26)=mob_id;
	WFIFOW(fd,30)=0x1400+progress; //Message to display
	WFIFOSET(fd, packet_len_table[0x20e]);
}

// ---------------------
// clif_guess_PacketVer
// ---------------------
// Parses a WantToConnection packet to try to identify which is the packet version used. [Skotlex]
static int clif_guess_PacketVer(int fd, int get_previous)
{
	static int packet_ver = -1;
	int cmd, packet_len, value; //Value is used to temporarily store account/char_id/sex
	RFIFOHEAD(fd);
	
	if (get_previous) //For quick reruns, since the normal code flow is to fetch this once to identify the packet version, then again in the wanttoconnect function. [Skotlex]
		return packet_ver;

	//By default, start searching on the default one. 
	packet_ver = clif_config.packet_db_ver;
	cmd = RFIFOW(fd,0);
	packet_len = RFIFOREST(fd);
	
	if (
		cmd == clif_config.connect_cmd[packet_ver] &&
		packet_len == packet_db[packet_ver][cmd].len &&
		((value = RFIFOB(fd, packet_db[packet_ver][cmd].pos[4])) == 0 ||	value == 1) &&
		(value = RFIFOL(fd, packet_db[packet_ver][cmd].pos[0])) > 700000 && //Account ID is valid
		 value <= max_account_id &&
		(value = RFIFOL(fd, packet_db[packet_ver][cmd].pos[1])) > 0 &&	//Char ID is valid
		 value <= max_char_id &&
		(int)RFIFOL(fd, packet_db[packet_ver][cmd].pos[2]) > 0	//Login 1 is a positive value (?)
		)
		return clif_config.packet_db_ver; //Default packet version found.
	
	for (packet_ver = MAX_PACKET_VER; packet_ver > 0; packet_ver--)
	{	//Start guessing the version, giving priority to the newer ones. [Skotlex]
		if (cmd != clif_config.connect_cmd[packet_ver] ||	//it is not a wanttoconnection for this version.
			packet_len != packet_db[packet_ver][cmd].len)	//The size of the wantoconnection packet does not matches.
			continue;
	
		if (
			(value = RFIFOL(fd, packet_db[packet_ver][cmd].pos[0])) < 700000 || value > max_account_id
			|| (value = RFIFOL(fd, packet_db[packet_ver][cmd].pos[1])) < 1 || value > max_char_id
			//What is login 1? In my tests it is a very very high value.
			|| (int)RFIFOL(fd, packet_db[packet_ver][cmd].pos[2]) < 1
			//This check seems redundant, all wanttoconnection packets have the gender on the very 
			//last byte of the packet.
			|| (value = RFIFOB(fd, packet_db[packet_ver][cmd].pos[4])) < 0 || value > 1
		)
			continue;
		
		return packet_ver; //This is our best guess.
	}
	packet_ver = -1;
	return -1;
}

// ------------
// clif_parse_*
// ------------
// パケット読み取って色々操作
/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_WantToConnection(int fd, struct map_session_data *sd)
{
	struct map_session_data *old_sd;
	int cmd, account_id, char_id, login_id1, sex;
	unsigned int client_tick; //The client tick is a tick, therefore it needs be unsigned. [Skotlex]
	int packet_ver;	// 5: old, 6: 7july04, 7: 13july04, 8: 26july04, 9: 9aug04/16aug04/17aug04, 10: 6sept04, 11: 21sept04, 12: 18oct04, 13: 25oct04 (by [Yor])
	RFIFOHEAD(fd);

	if (sd) {
		if (battle_config.error_log)
			ShowError("clif_parse_WantToConnection : invalid request (character already logged in)?\n");
		return;
	}

	packet_ver = clif_guess_PacketVer(fd, 1);
	cmd = RFIFOW(fd,0);
	
	if (packet_ver > 0)
	{
		account_id	= RFIFOL(fd, packet_db[packet_ver][cmd].pos[0]);
		char_id		= RFIFOL(fd, packet_db[packet_ver][cmd].pos[1]);
		login_id1	= RFIFOL(fd, packet_db[packet_ver][cmd].pos[2]);
		client_tick	= RFIFOL(fd, packet_db[packet_ver][cmd].pos[3]);
		sex			= RFIFOB(fd, packet_db[packet_ver][cmd].pos[4]);
			
		if ((old_sd = map_id2sd(account_id)) != NULL)
		{	// if same account already connected, we disconnect the 2 sessions
			//Check for characters with no connection (includes those that are using autotrade) [durf],[Skotlex]
			if (!old_sd->fd)
				map_quit(old_sd);
			else 
				clif_authfail_fd(old_sd->fd, 2); // same id
			clif_authfail_fd(fd, 8); // still recognizes last connection
		} else {
			sd = (struct map_session_data*)aCalloc(1, sizeof(struct map_session_data));

			sd->fd = fd;
			sd->packet_ver = packet_ver;
			session[fd]->session_data = sd;

			pc_setnewpc(sd, account_id, char_id, login_id1, client_tick, sex, fd);
			WFIFOHEAD(fd,4);
			WFIFOL(fd,0) = sd->bl.id;
			WFIFOSET(fd,4);

			chrif_authreq(sd);
		}
	}
	return;
}

static int clif_nighttimer(int tid, unsigned int tick, int id, int data)
{
	TBL_PC *sd;
	sd=map_id2sd(id);
	if (!sd) return 0;

	//Check if character didn't instant-warped after logging in.
	if (sd->bl.prev!=NULL) {
		sd->state.night = 1;
 		clif_status_load(&sd->bl, SI_NIGHT, 1);
	}
	return 0;
}

/*==========================================
 * 007d クライアント側マップ読み込み完了
 * map侵入時に必要なデータを全て送りつける
 *------------------------------------------
 */
void clif_parse_LoadEndAck(int fd,struct map_session_data *sd)
{
	int i;
	
	if(sd->bl.prev != NULL)
		return;

	if(sd->npc_id) npc_event_dequeue(sd);
	clif_skillinfoblock(sd);
	pc_checkitem(sd);

	// loadendack時
	// next exp
	clif_updatestatus(sd,SP_NEXTBASEEXP);
	clif_updatestatus(sd,SP_NEXTJOBEXP);
	// skill point
	clif_updatestatus(sd,SP_SKILLPOINT);
	// item
	clif_itemlist(sd);
	clif_equiplist(sd);
	// cart
	if(pc_iscarton(sd)){
		clif_cart_itemlist(sd);
		clif_cart_equiplist(sd);
		clif_updatestatus(sd,SP_CARTINFO);
	}
	// param all
	clif_initialstatus(sd);
	if(battle_config.pc_invincible_time > 0) {
		if(map_flag_gvg(sd->bl.m))
			pc_setinvincibletimer(sd,battle_config.pc_invincible_time<<1);
		else
			pc_setinvincibletimer(sd,battle_config.pc_invincible_time);
	}

	map_addblock(&sd->bl);	// ブロック登録
	clif_spawn(&sd->bl);	// spawn

	// party
	party_send_movemap(sd);
	// guild
	guild_send_memberinfoshort(sd,1);

	// weight max , now
	clif_updatestatus(sd,SP_MAXWEIGHT);
	clif_updatestatus(sd,SP_WEIGHT);

	// Show hp after displacement [LuzZza]
	if(sd->status.party_id)
	    clif_party_hp(sd);

	sd->state.using_fake_npc = 0;

	// pvp
	//if(sd->pvp_timer!=-1 && !battle_config.pk_mode) /PVP Client crash fix* Removed timer deletion
	//	delete_timer(sd->pvp_timer,pc_calc_pvprank_timer);
	if(map[sd->bl.m].flag.pvp){
		if(!battle_config.pk_mode) { // remove pvp stuff for pk_mode [Valaris]
			if (sd->pvp_timer == -1)
				sd->pvp_timer=add_timer(gettick()+200,pc_calc_pvprank_timer,sd->bl.id,0);
			sd->pvp_rank=0;
			sd->pvp_lastusers=0;
			sd->pvp_point=5;
			sd->pvp_won=0;
			sd->pvp_lost=0;
		}
		clif_set0199(fd,1);
	} else {
		sd->pvp_timer=-1;
		// set flag, if it's a duel [LuzZza]
		if(sd->duel_group)
			clif_set0199(fd, 1);
	}
	if(map_flag_gvg(sd->bl.m))
		clif_set0199(fd,3);

	// pet
	if(sd->status.pet_id > 0 && sd->pd) {
		map_addblock(&sd->pd->bl);
		clif_spawn(&sd->pd->bl);
		clif_send_petdata(sd,0,0);
		clif_send_petdata(sd,5,battle_config.pet_hair_style);
		clif_send_petstatus(sd);
	}

	//homunculus [blackhole89]
	if(sd->hd && sd->hd->alive) {
		map_addblock(&sd->hd->bl);
		clif_spawn(&sd->hd->bl);
		clif_homunack(sd);
		clif_homuninfo(sd);
		clif_homuninfo(sd); //for some reason, at least older clients want this sent twice
		clif_homunskillinfoblock(sd);
	}

	if(sd->state.connect_new) {
		sd->state.connect_new = 0;
		//Delayed night effect on log-on fix for the glow-issue. Thanks to Larry.
		if (night_flag) {
			char tmpstr[1024];
			strcpy(tmpstr, msg_txt(500)); // Actually, it's the night...
			clif_wis_message(sd->fd, wisp_server_name, tmpstr, strlen(tmpstr)+1);
			
			if (map[sd->bl.m].flag.nightenabled)
				add_timer(gettick()+1000,clif_nighttimer,sd->bl.id,0);
		}

//		if(sd->status.class_ != sd->vd.class_)
//			clif_refreshlook(&sd->bl,sd->bl.id,LOOK_BASE,sd->vd.class_,SELF);

		if (sd->sc.option&OPTION_FALCON)
			clif_status_load(&sd->bl, SI_FALCON, 1);
		if (sd->sc.option&OPTION_RIDING)
			clif_status_load(&sd->bl, SI_RIDING, 1);

		//Auron reported that This skill only triggers when you logon on the map o.O [Skotlex]
		if ((i = pc_checkskill(sd,SG_KNOWLEDGE)) > 0) {
			if(sd->bl.m == sd->feel_map[0].m
				|| sd->bl.m == sd->feel_map[1].m
				|| sd->bl.m == sd->feel_map[2].m)
				sc_start(&sd->bl, SC_KNOWLEDGE, 100, i, skill_get_time(SG_KNOWLEDGE, i));
		}

		if(sd->status.pet_id > 0 && sd->pd && sd->pet.intimate > 900)
			clif_pet_emotion(sd->pd,(sd->pd->class_ - 100)*100 + 50 + pet_hungry_val(sd));
		//Removed, for some reason chars get stuck on map-change when you send this packet!? [Skotlex]
		//[LuzZza]
		//clif_guild_send_onlineinfo(sd);

	} else
	//New 'night' effect by dynamix [Skotlex]
	if (night_flag && map[sd->bl.m].flag.nightenabled)
	{	//Display night.
		if (sd->state.night) //It must be resent because otherwise players get this annoying aura...
			clif_status_load(&sd->bl, SI_NIGHT, 0);
		else
			sd->state.night = 1;
		clif_status_load(&sd->bl, SI_NIGHT, 1);
	} else if (sd->state.night) { //Clear night display.
		sd->state.night = 0;
		clif_status_load(&sd->bl, SI_NIGHT, 0);
	}

	// view equipment item
#if PACKETVER < 4
	clif_changelook(&sd->bl,LOOK_WEAPON,sd->status.weapon);
	clif_changelook(&sd->bl,LOOK_SHIELD,sd->status.shield);
#else
	clif_changelook(&sd->bl,LOOK_WEAPON,0);
#endif

	if(sd->vd.cloth_color)
		clif_refreshlook(&sd->bl,sd->bl.id,LOOK_CLOTHES_COLOR,sd->vd.cloth_color,SELF);

	if(battle_config.muting_players && sd->status.manner < 0 && battle_config.manner_system)
		sc_start(&sd->bl,SC_NOCHAT,100,0,0);

// Lance
	npc_script_event(sd, NPCE_LOADMAP);

	if (pc_checkskill(sd, SG_DEVIL) && !pc_nextjobexp(sd))
		clif_status_load(&sd->bl, SI_DEVIL, 1);  //blindness [Komurka]

	clif_weather_check(sd);
	
	map_foreachinarea(clif_getareachar,sd->bl.m,
		sd->bl.x-AREA_SIZE,sd->bl.y-AREA_SIZE,sd->bl.x+AREA_SIZE,sd->bl.y+AREA_SIZE,
		BL_ALL,sd);
	
	// For automatic triggering of NPCs after map loading (so you don't need to walk 1 step first)
	if (map_getcell(sd->bl.m,sd->bl.x,sd->bl.y,CELL_CHKNPC))
		npc_touch_areanpc(sd,sd->bl.m,sd->bl.x,sd->bl.y);
	else
		sd->areanpc_id = 0;

	if (pc_isdead(sd)) //In case you warped dead.
		clif_clearchar_area(&sd->bl, 1);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_TickSend(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);

	sd->client_tick=RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);
	sd->server_tick = gettick();
	clif_servertick(sd);
}

static int clif_walktoxy_timer(int tid, unsigned int tick, int id, int data)
{
	struct map_session_data *sd;
	short x,y;

	if (!session[id] || (sd = session[id]->session_data) == NULL)
		return 0;
	
	if (!unit_can_move(&sd->bl))
		return 0;

	x = data>>16;
	y = data&0xffff;

	unit_walktoxy(&sd->bl, x, y, 0);
	return 1;
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_WalkToXY(int fd, struct map_session_data *sd) {
	int x, y;
	int cmd;
	unsigned int tick;
	RFIFOHEAD(fd);

	if (pc_isdead(sd)) {
		clif_clearchar_area(&sd->bl, 1);
		return;
	}

	if (clif_cant_act(sd) && sd->sc.opt1 != OPT1_STONEWAIT)
		return;

	if(sd->sc.count && sd->sc.data[SC_RUN].timer != -1)
		return;

	pc_stop_attack(sd);

	if (sd->invincible_timer != -1)
		pc_delinvincibletimer(sd);

	cmd = RFIFOW(fd,0);
	x = RFIFOB(fd,packet_db[sd->packet_ver][cmd].pos[0]) * 4 +
		(RFIFOB(fd,packet_db[sd->packet_ver][cmd].pos[0] + 1) >> 6);
	y = ((RFIFOB(fd,packet_db[sd->packet_ver][cmd].pos[0]+1) & 0x3f) << 4) +
		(RFIFOB(fd,packet_db[sd->packet_ver][cmd].pos[0] + 2) >> 4);
	//Set last idle time... [Skotlex]
	sd->idletime = last_tick;
	
	tick = gettick();
	if (DIFF_TICK(sd->ud.canmove_tick, tick) > 0 &&
		DIFF_TICK(sd->ud.canmove_tick, tick) < 2000)
  	{	// Delay walking command. [Skotlex]
		add_timer(sd->ud.canmove_tick+1, clif_walktoxy_timer, fd, (x<<16)|y);
		return;
	}
	if (!unit_can_move(&sd->bl))
		return;
	unit_walktoxy(&sd->bl, x, y, 0);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_QuitGame(int fd, struct map_session_data *sd) {
	WFIFOHEAD(fd,packet_len_table[0x18b]);
	WFIFOW(fd,0) = 0x18b;

	/*	Rovert's prevent logout option fixed [Valaris]	*/
	if (sd->sc.data[SC_CLOAKING].timer==-1 && sd->sc.data[SC_HIDING].timer==-1 &&
		(!battle_config.prevent_logout || DIFF_TICK(gettick(), sd->canlog_tick) > battle_config.prevent_logout)
	) {
		clif_setwaitclose(fd);
		WFIFOW(fd,2)=0;
	} else {
		WFIFOW(fd,2)=1;
	}
	WFIFOSET(fd,packet_len_table[0x18b]);
}


/*==========================================
 *
 *------------------------------------------
 */
void check_fake_id(int fd, struct map_session_data *sd, int target_id) {
	// if player asks for the fake player (only bot and modified client can see a hiden player)
/*	if (target_id == server_char_id) {
		char message_to_gm[strlen(msg_txt(536)) + strlen(msg_txt(540)) + strlen(msg_txt(507)) + strlen(msg_txt(508))];
		sprintf(message_to_gm, msg_txt(536), sd->status.name, sd->status.account_id); // Character '%s' (account: %d) try to use a bot (it tries to detect a fake player).
		intif_wis_message_to_gm(wisp_server_name, battle_config.hack_info_GM_level, message_to_gm);
		// if we block people
		if (battle_config.ban_bot < 0) {
			chrif_char_ask_name(-1, sd->status.name, 1, 0, 0, 0, 0, 0, 0); // type: 1 - block
			clif_setwaitclose(sd->fd); // forced to disconnect because of the hack
			// message about the ban
			sprintf(message_to_gm, msg_txt(540)); //  This player has been definitivly blocked.
		// if we ban people
		} else if (battle_config.ban_bot > 0) {
			chrif_char_ask_name(-1, sd->status.name, 2, 0, 0, 0, 0, battle_config.ban_bot, 0); // type: 2 - ban (year, month, day, hour, minute, second)
			clif_setwaitclose(sd->fd); // forced to disconnect because of the hack
			// message about the ban
			sprintf(message_to_gm, msg_txt(507), battle_config.ban_bot); //  This player has been banned for %d minute(s).
		} else { // impossible to display: we don't send fake player if battle_config.ban_bot is == 0
			// message about the ban
			sprintf(message_to_gm, msg_txt(508)); //  This player hasn't been banned (Ban option is disabled).
		}
		intif_wis_message_to_gm(wisp_server_name, battle_config.hack_info_GM_level, message_to_gm);
		// send this info cause the bot ask until get an answer, damn spam
		memset(WPACKETP(0), 0, packet_len_table[0x95]);
		WPACKETW(0) = 0x95;
		WPACKETL(2) = server_char_id;
		strncpy(WPACKETP(6), sd->status.name, 24);
		SENDPACKET(fd, packet_len_table[0x95]);
		// take fake player out of screen
		WPACKETW(0) = 0x80;
		WPACKETL(2) = server_char_id;
		WPACKETB(6) = 0;
		SENDPACKET(fd, packet_len_table[0x80]);
		// take fake mob out of screen
		WPACKETW(0) = 0x80;
		WPACKETL(2) = server_fake_mob_id;
		WPACKETB(6) = 0;
		SENDPACKET(fd, packet_len_table[0x80]);
	}

	// if player asks for the fake mob (only bot and modified client can see a hiden mob)
	if (target_id == server_fake_mob_id) {
		int fake_mob;
		char message_to_gm[strlen(msg_txt(537)) + strlen(msg_txt(540)) + strlen(msg_txt(507)) + strlen(msg_txt(508))];
		sprintf(message_to_gm, msg_txt(537), sd->status.name, sd->status.account_id); // Character '%s' (account: %d) try to use a bot (it tries to detect a fake mob).
		intif_wis_message_to_gm(wisp_server_name, battle_config.hack_info_GM_level, message_to_gm);
		// if we block people
		if (battle_config.ban_bot < 0) {
			chrif_char_ask_name(-1, sd->status.name, 1, 0, 0, 0, 0, 0, 0); // type: 1 - block
			clif_setwaitclose(sd->fd); // forced to disconnect because of the hack
			// message about the ban
			sprintf(message_to_gm, msg_txt(540)); //  This player has been definitivly blocked.
		// if we ban people
		} else if (battle_config.ban_bot > 0) {
			chrif_char_ask_name(-1, sd->status.name, 2, 0, 0, 0, 0, battle_config.ban_bot, 0); // type: 2 - ban (year, month, day, hour, minute, second)
			clif_setwaitclose(sd->fd); // forced to disconnect because of the hack
			// message about the ban
			sprintf(message_to_gm, msg_txt(507), battle_config.ban_bot); //  This player has been banned for %d minute(s).
		} else { // impossible to display: we don't send fake player if battle_config.ban_bot is == 0
			// message about the ban
			sprintf(message_to_gm, msg_txt(508)); //  This player hasn't been banned (Ban option is disabled).
		}
		intif_wis_message_to_gm(wisp_server_name, battle_config.hack_info_GM_level, message_to_gm);
		// send this info cause the bot ask until get an answer, damn spam
		memset(WPACKETP(0), 0, packet_len_table[0x95]);
		WPACKETW(0) = 0x95;
		WPACKETL(2) = server_fake_mob_id;
		fake_mob = fake_mob_list[(sd->bl.m + sd->fd + sd->status.char_id) % (sizeof(fake_mob_list) / sizeof(fake_mob_list[0]))]; // never same mob
		if (!mobdb_checkid(fake_mob))
			fake_mob = 1002; // poring (default)
		strncpy(WPACKETP(6), mob_db[fake_mob].name, 24);
		SENDPACKET(fd, packet_len_table[0x95]);
		// take fake mob out of screen
		WPACKETW(0) = 0x80;
		WPACKETL(2) = server_fake_mob_id;
		WPACKETB(6) = 0;
		SENDPACKET(fd, packet_len_table[0x80]);
		// take fake player out of screen
		WPACKETW(0) = 0x80;
		WPACKETL(2) = server_char_id;
		WPACKETB(6) = 0;
		SENDPACKET(fd, packet_len_table[0x80]);
	}
*/
	return;
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_GetCharNameRequest(int fd, struct map_session_data *sd) {
	int account_id;
	struct block_list* bl;
	struct status_change *sc;
	RFIFOHEAD(fd);
	
	account_id = RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);

	if(account_id<0 && -account_id == sd->bl.id) // for disguises [Valaris]
		account_id= sd->bl.id;

	bl = map_id2bl(account_id);
	//Is this possible? Lagged clients could request names of already gone mobs/players. [Skotlex]
	if (!bl) return;

	sc = status_get_sc(bl);
	if (sc && sc->option&OPTION_INVISIBLE && !disguised(bl))
	{
		//Asked name of invisible player, this shouldn't be possible!
		//Possible bot? Thanks to veider and qspirit
		//FIXME: Still isn't perfected as clients keep asking for this on legitimate situations.
		unsigned char gm_msg[256];
		sprintf(gm_msg, "Hack on NameRequest: character '%s' (account: %d) requests name of invisible chars.", sd->status.name, sd->status.account_id);
		ShowWarning(gm_msg);
		 // information is sended to all online GM
		intif_wis_message_to_gm(wisp_server_name, battle_config.hack_info_GM_level, gm_msg);
		return;
	}
	clif_charnameack(fd, bl);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_GlobalMessage(int fd, struct map_session_data *sd) { // S 008c <len>.w <str>.?B
	unsigned char *message, *buf, buf2[128];
	RFIFOHEAD(fd);
	WFIFOHEAD(fd, RFIFOW(fd,2) + 4);

	message = (unsigned char*)RFIFOP(fd,4);
	if (strlen(message) < strlen(sd->status.name) || //If the incoming string is too short...
		strncmp(message, sd->status.name, strlen(sd->status.name)) != 0) //Or the name does not matches...
	{
		unsigned char gm_msg[256];
		ShowWarning("Hack on global message: character '%s' (account: %d), use an other name to send a (normal) message.\n", sd->status.name, sd->status.account_id);
		// information is sended to all online GM
		sprintf(gm_msg, "Hack on global message (normal message): character '%s' (account: %d) uses another name.", sd->status.name, sd->status.account_id);
		intif_wis_message_to_gm(wisp_server_name, battle_config.hack_info_GM_level, gm_msg);
		
		if (strlen(message) == 0)
			strcpy(gm_msg, " This player sends a void name and a void message.");
		else
			snprintf(gm_msg, 255, " This player sends (name:message): '%128s'.", message);
		intif_wis_message_to_gm(wisp_server_name, battle_config.hack_info_GM_level, gm_msg);
		// message about the ban
		if (battle_config.ban_spoof_namer > 0)
			sprintf(gm_msg, " This player has been banned for %d minute(s).", battle_config.ban_spoof_namer);
		else
			sprintf(gm_msg, " This player hasn't been banned (Ban option is disabled).");
		intif_wis_message_to_gm(wisp_server_name, battle_config.hack_info_GM_level, gm_msg);

		// if we ban people
		if (battle_config.ban_spoof_namer > 0) {
			chrif_char_ask_name(-1, sd->status.name, 2, 0, 0, 0, 0, battle_config.ban_spoof_namer, 0); // type: 2 - ban (year, month, day, hour, minute, second)
			clif_setwaitclose(fd); // forced to disconnect because of the hack
		}
		else
			session[fd]->eof = 1; //Disconnect them too, bad packets can cause problems down the road. [Skotlex]
		return;
	}
	
	if ((is_atcommand(fd, sd, message, 0) != AtCommand_None) ||
		(is_charcommand(fd, sd, message,0) != CharCommand_None))
		return;
	if (sd->sc.count &&
		(sd->sc.data[SC_BERSERK].timer != -1 || sd->sc.data[SC_NOCHAT].timer != -1 ))
		return;

	if (RFIFOW(fd,2)+4 < 128)
		buf = buf2; //Use a static buffer.
	else
		buf = (unsigned char*)aMallocA((RFIFOW(fd,2) + 4)*sizeof(char));

	// send message to others
	WBUFW(buf,0) = 0x8d;
	WBUFW(buf,2) = RFIFOW(fd,2) + 4; // len of message - 4 + 8
	WBUFL(buf,4) = sd->bl.id;
	memcpy(WBUFP(buf,8), message, RFIFOW(fd,2) - 4);
	clif_send(buf, WBUFW(buf,2), &sd->bl, sd->chatID ? CHAT_WOS : AREA_CHAT_WOC);

	if(buf != buf2) aFree(buf);

	// send back message to the speaker
	memcpy(WFIFOP(fd,0), RFIFOP(fd,0), RFIFOW(fd,2));
	WFIFOW(fd,0) = 0x8e;
	WFIFOSET(fd, WFIFOW(fd,2));

#ifdef PCRE_SUPPORT
	map_foreachinrange(npc_chat_sub, &sd->bl, AREA_SIZE, BL_NPC, message, strlen(message), &sd->bl);
	map_foreachinrange(mob_chat_sub, &sd->bl, AREA_SIZE, BL_MOB, message, strlen(message), &sd->bl);
#endif

	// Celest
	if ((sd->class_&MAPID_UPPERMASK) == MAPID_SUPER_NOVICE) { //Super Novice.
		int next = pc_nextbaseexp(sd);
		if (next > 0 && (sd->status.base_exp * 1000 / next)% 100 == 0) {
			switch (sd->state.snovice_flag) {
			case 0:
				if (strstr(message, msg_txt(504)))
					sd->state.snovice_flag++;
				break;
			case 1:
				sprintf(buf2, msg_txt(505), sd->status.name);
				if (strstr(message, buf2))
					sd->state.snovice_flag++;
				break;
			case 2:
				if (strstr(message, msg_txt(506)))
					sd->state.snovice_flag++;
				break;
			case 3:
				if (skillnotok(MO_EXPLOSIONSPIRITS,sd))
					break; //Do not override the noskill mapflag. [Skotlex]
				clif_skill_nodamage(&sd->bl,&sd->bl,MO_EXPLOSIONSPIRITS,-1,
					sc_start(&sd->bl,SkillStatusChangeTable[MO_EXPLOSIONSPIRITS],100,
						17,skill_get_time(MO_EXPLOSIONSPIRITS,1))); //Lv17-> +50 critical (noted by Poki) [Skotlex]
				sd->state.snovice_flag = 0;
				break;
			}
		}
	}
	return;
}

int clif_message(struct block_list *bl, char* msg)
{
	unsigned short msg_len = strlen(msg) + 1;
	unsigned char buf[256];

	nullpo_retr(0, bl);

	WBUFW(buf, 0) = 0x8d;
	WBUFW(buf, 2) = msg_len + 8;
	WBUFL(buf, 4) = bl->id;
	memcpy(WBUFP(buf, 8), msg, msg_len);

	clif_send(buf, WBUFW(buf,2), bl, AREA_CHAT_WOC);	// by Gengar

	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_MapMove(int fd, struct map_session_data *sd) {
// /m /mapmove (as @rura GM command)
	char output[30]; // 17+4+4=26, 30 max.
	char map_name[MAP_NAME_LENGTH]; //Err... map names are 15+'\0' in size, not 16+'\0' [Skotlex]
	RFIFOHEAD(fd);

	if ((battle_config.atc_gmonly == 0 || pc_isGM(sd)) &&
	    (pc_isGM(sd) >= get_atcommand_level(AtCommand_MapMove))) {
		memcpy(map_name, RFIFOP(fd,2), MAP_NAME_LENGTH-1);
		map_name[MAP_NAME_LENGTH-1]='\0';
		sprintf(output, "%s %d %d", map_name, RFIFOW(fd,18), RFIFOW(fd,20));
		atcommand_rura(fd, sd, "@rura", output);
	}

	return;
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_changed_dir(struct block_list *bl) {
	unsigned char buf[64];

	WBUFW(buf,0) = 0x9c;
	WBUFL(buf,2) = bl->id;
	WBUFW(buf,6) = bl->type==BL_PC?((TBL_PC*)bl)->head_dir:0;
	WBUFB(buf,8) = unit_getdir(bl);

	clif_send(buf, packet_len_table[0x9c], bl, AREA_WOS);
	if (disguised(bl)) {
		WBUFL(buf,2) = -bl->id;
		WBUFW(buf,6) = 0;
		clif_send(buf, packet_len_table[0x9c], bl, SELF);
	}

	return;
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_ChangeDir(int fd, struct map_session_data *sd) {
	unsigned char headdir, dir;
	RFIFOHEAD(fd);

	headdir = RFIFOB(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);
	dir = RFIFOB(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[1]);
	pc_setdir(sd, dir, headdir);

	clif_changed_dir(&sd->bl);
	return;
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_Emotion(int fd, struct map_session_data *sd) {
	unsigned char buf[64];
	RFIFOHEAD(fd);

	if (battle_config.basic_skill_check == 0 || pc_checkskill(sd, NV_BASIC) >= 2) {
		if (RFIFOB(fd,2) == 34) {// prevent use of the mute emote [Valaris]
			clif_skill_fail(sd, 1, 0, 1);
			return;
		}
		// fix flood of emotion icon (ro-proxy): flood only the hacker player
		if (sd->emotionlasttime >= time(NULL)) {
			sd->emotionlasttime = time(NULL) + 1; // not more than 1 per second (using /commands the client can spam it)
			clif_skill_fail(sd, 1, 0, 1);
			return;
		}
		sd->emotionlasttime = time(NULL) + 1; // not more than 1 per second (using /commands the client can spam it)
		
		WBUFW(buf,0) = 0xc0;
		WBUFL(buf,2) = sd->bl.id;
		WBUFB(buf,6) = RFIFOB(fd,2);
		clif_send(buf, packet_len_table[0xc0], &sd->bl, AREA);
	} else
		clif_skill_fail(sd, 1, 0, 1);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_HowManyConnections(int fd, struct map_session_data *sd) {
	WFIFOHEAD(fd,packet_len_table[0xc2]);
	WFIFOW(fd,0) = 0xc2;
	WFIFOL(fd,2) = map_getusers();
	WFIFOSET(fd,packet_len_table[0xc2]);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_ActionRequest(int fd, struct map_session_data *sd) {
	unsigned int tick;
	unsigned char buf[64];
	int action_type, target_id;
	RFIFOHEAD(fd);

	if (pc_isdead(sd)) {
		clif_clearchar_area(&sd->bl, 1);
		return;
	}

	if (sd->sc.count &&
		(sd->sc.data[SC_TRICKDEAD].timer != -1 ||
	 	sd->sc.data[SC_AUTOCOUNTER].timer != -1 ||
		sd->sc.data[SC_BLADESTOP].timer != -1))
		return;

	tick = gettick();

	pc_stop_walking(sd, 1);
	pc_stop_attack(sd);

	target_id = RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);
	action_type = RFIFOB(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[1]);

	if(target_id<0 && -target_id == sd->bl.id) // for disguises [Valaris]
		target_id = sd->bl.id;
		
	switch(action_type) {
	case 0x00: // once attack
	case 0x07: // continuous attack

		if (clif_cant_act(sd) || sd->sc.option&OPTION_HIDE)
			return;

		if(sd->sc.option&OPTION_WEDDING || sd->vd.class_ == JOB_XMAS)
			return;

		if (!battle_config.sdelay_attack_enable && pc_checkskill(sd, SA_FREECAST) <= 0) {
			if (DIFF_TICK(tick, sd->ud.canact_tick) < 0) {
				clif_skill_fail(sd, 1, 4, 0);
				return;
			}
		}
		if (sd->invincible_timer != -1)
			pc_delinvincibletimer(sd);

		sd->idletime = last_tick;
		unit_attack(&sd->bl, target_id, action_type != 0);
		break;
	case 0x02: // sitdown
		if (battle_config.basic_skill_check && pc_checkskill(sd, NV_BASIC) < 3) {
			clif_skill_fail(sd, 1, 0, 2);
			break;
		}
		if(pc_issit(sd)) {
			//Bugged client? Just refresh them.
			WBUFW(buf, 0) = 0x8a;
			WBUFL(buf, 2) = sd->bl.id;
			WBUFB(buf,26) = 2;
			clif_send(buf, packet_len_table[0x8a], &sd->bl, SELF);
			return;
		}

		if (sd->ud.skilltimer != -1 || sd->sc.opt1)
			break;

		if (sd->sc.count && (
			sd->sc.data[SC_DANCING].timer != -1 ||
			(sd->sc.data[SC_GRAVITATION].timer != -1 && sd->sc.data[SC_GRAVITATION].val3 == BCT_SELF)
		)) //No sitting during these states neither.
		break;
		pc_setsit(sd);
		skill_gangsterparadise(sd, 1); // ギャングスターパラダイス設定 fixed Valaris
		skill_rest(sd, 1); // TK_HPTIME sitting down mode [Dralnu]
		clif_sitting(sd);
		break;
	case 0x03: // standup
		if (!pc_issit(sd)) {
			//Bugged client? Just refresh them.
			WBUFW(buf, 0) = 0x8a;
			WBUFL(buf, 2) = sd->bl.id;
			WBUFB(buf,26) = 3;
			clif_send(buf, packet_len_table[0x8a], &sd->bl, SELF);
			return;
		}
		pc_setstand(sd);
		skill_gangsterparadise(sd, 0); // ギャングスターパラダイス解除 fixed Valaris
		skill_rest(sd, 0); // TK_HPTIME standing up mode [Dralnu]
		WBUFW(buf, 0) = 0x8a;
		WBUFL(buf, 2) = sd->bl.id;
		WBUFB(buf,26) = 3;
		clif_send(buf, packet_len_table[0x8a], &sd->bl, AREA);
		break;
	}
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_Restart(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);

	switch(RFIFOB(fd,2)) {
	case 0x00:
		if (pc_isdead(sd)) {
			pc_setstand(sd);
			pc_setrestartvalue(sd, 3);
			pc_setpos(sd, sd->status.save_point.map, sd->status.save_point.x, sd->status.save_point.y, 2);
		}
		// in case the player's status somehow wasn't updated yet [Celest]
		else if (sd->status.hp <= 0)
			pc_setdead(sd);
		break;
	case 0x01:
		/*	Rovert's Prevent logout option - Fixed [Valaris]	*/
		if (!battle_config.prevent_logout || DIFF_TICK(gettick(), sd->canlog_tick) > battle_config.prevent_logout)
		{
			//map_quit(sd);	//A clif_quitsave is sent inmediately after this, so no need to quit yet. [Skotlex]
			chrif_charselectreq(sd);
		} else {
                        WFIFOHEAD(fd,packet_len_table[0x18b]);
			WFIFOW(fd,0)=0x18b;
			WFIFOW(fd,2)=1;

			WFIFOSET(fd,packet_len_table[0x018b]);
		}
		break;
	}
}

/*==========================================
 * Transmission of a wisp (S 0096 <len>.w <nick>.24B <message>.?B)
 *------------------------------------------
 */
void clif_parse_Wis(int fd, struct map_session_data *sd) { // S 0096 <len>.w <nick>.24B <message>.?B // rewritten by [Yor]
	char *gm_command;
	struct map_session_data *dstsd;
	int i=0;
	struct npc_data *npc;
	char split_data[10][50];
	int j=0,k=0;
	char target[NAME_LENGTH+1];
	char output[256];  
	RFIFOHEAD(fd);

	//printf("clif_parse_Wis: message: '%s'.\n", RFIFOP(fd,28));

	gm_command = (char*)aMallocA((strlen((const char*)RFIFOP(fd,28)) + 28)*sizeof(char)); // 24+3+(RFIFOW(fd,2)-28)+1 or 24+3+(strlen(RFIFOP(fd,28))+1 (size can be wrong with hacker)

	sprintf(gm_command, "%s : %s", sd->status.name, RFIFOP(fd,28));
	if ((is_charcommand(fd, sd, gm_command, 0) != CharCommand_None) ||
		(is_atcommand(fd, sd, gm_command, 0) != AtCommand_None)) {
		if(gm_command) aFree(gm_command);
		return;
	}
	if(gm_command) aFree(gm_command);
	if (sd->sc.count &&
		(sd->sc.data[SC_BERSERK].timer!=-1 || sd->sc.data[SC_NOCHAT].timer != -1))
		return;

	memcpy(&target,RFIFOP(fd, 4),NAME_LENGTH);
	target[NAME_LENGTH]='\0';
	
	//Chat Logging type 'W' / Whisper
	if(log_config.chat&1 //we log everything then
		|| ( log_config.chat&2 //if Whisper bit is on
		&& ( !agit_flag || !(log_config.chat&16) ))) //if WOE ONLY flag is off or AGIT is OFF
		log_chat("W", 0, sd->status.char_id, sd->status.account_id, (char*)mapindex_id2name(sd->mapindex), sd->bl.x, sd->bl.y, target, (char*)RFIFOP(fd, 28));


	//-------------------------------------------------------//
	//   Lordalfa - Paperboy - To whisper NPC commands       //
	//-------------------------------------------------------//
	if (target[0] && (strncasecmp(target,"NPC:",4) == 0) && (strlen(target) >4))   {
		char *whisper_tmp = target+4; //Skip the NPC: string part.
		if ((npc = npc_name2id(whisper_tmp)))	
		{
			whisper_tmp=(char *)aMallocA((strlen((char *)(RFIFOP(fd,28)))+1)*sizeof(char));
		   
			sprintf(whisper_tmp, "%s", (const char*)RFIFOP(fd,28));  
			for( j=0;whisper_tmp[j]!='\0';j++)
			{
				if(whisper_tmp[j]!='#')
				{
					split_data[i][j-k]=whisper_tmp[j];
				}
				else
				{
					split_data[i][j-k]='\0';
					k=j+1;
					i++;
				}
			} // Splits the message using '#' as separators
			split_data[i][j-k]='\0';
			
			aFree(whisper_tmp);
			whisper_tmp=(char *)aMallocA(15*sizeof(char));
			
			for (j=0;j<=10;j++)
			{
				sprintf(whisper_tmp, "@whispervar%d$", j);
				set_var(sd,whisper_tmp,(char *) split_data[j]);        
			}//You don't need to zero them, just reset them [Kevin]
			
			aFree(whisper_tmp);
			whisper_tmp=(char *)aMallocA((strlen(npc->name)+18)*sizeof(char));
			
			sprintf(whisper_tmp, "%s::OnWhisperGlobal", npc->name);
			npc_event(sd,whisper_tmp,0); // Calls the NPC label 

			aFree(whisper_tmp); //I rewrote it a little to use memory allocation, a bit more stable =P  [Kevin]
			return;     
		} //should have just removed the one below that was a my bad =P
	}		
	
	// Main chat [LuzZza]
	if(strcmpi(target, main_chat_nick) == 0) {
		if(!sd->state.mainchat) {
			clif_displaymessage(fd, msg_txt(388)); // You should enable main chat with "@main on" command.
			return;
		}
		if (sd->sc.data[SC_NOCHAT].timer != -1) {
			clif_displaymessage(fd, msg_txt(387));
			return;
		}
		sprintf(output, msg_txt(386), sd->status.name, (char *)RFIFOP(fd,28));
		intif_announce(output, strlen(output) + 1, 0xFE000000, 0);
		return;
	}

	// searching destination character
	dstsd = map_nick2sd(target);
	// player is not on this map-server
	if (dstsd == NULL ||
	// At this point, don't send wisp/page if it's not exactly the same name, because (example)
	// if there are 'Test' player on an other map-server and 'test' player on this map-server,
	// and if we ask for 'Test', we must not contact 'test' player
	// so, we send information to inter-server, which is the only one which decide (and copy correct name).
	    strcmp(dstsd->status.name, target) != 0) // not exactly same name
		// send message to inter-server
		intif_wis_message(sd, target, (char*)RFIFOP(fd,28), RFIFOW(fd,2)-28);
	// player is on this map-server
	else {
		// if you send to your self, don't send anything to others
		if (dstsd->fd == fd) // but, normaly, it's impossible!
			clif_wis_message(fd, wisp_server_name, "You can not page yourself. Sorry.", strlen("You can not page yourself. Sorry.") + 1);
		// otherwise, send message and answer immediatly
		else {
			if (dstsd->ignoreAll == 1) {
				if (dstsd->sc.option & OPTION_INVISIBLE && pc_isGM(sd) < pc_isGM(dstsd))
					clif_wis_end(fd, 1); // 1: target character is not loged in
				else
					clif_wis_end(fd, 3); // 3: everyone ignored by target
			} else {
				// if player ignore the source character
				for(i = 0; i < MAX_IGNORE_LIST; i++)
					if (strcmp(dstsd->ignore[i].name, sd->status.name) == 0) {
						clif_wis_end(fd, 2);	// 2: ignored by target
						break;
					}
				// if source player not found in ignore list
				if (i == MAX_IGNORE_LIST) {
					if(strlen(dstsd->away_message) > 0) { // Send away automessage [LuzZza]
						//(Automessage has been sent)
						sprintf(output, "%s %s", (char*)RFIFOP(fd,28),msg_txt(543));
						clif_wis_message(dstsd->fd, sd->status.name, output, strlen(output) + 1);
						clif_wis_end(fd, 0); // 0: success to send wisper
						if(dstsd->state.autotrade)
							//"Away [AT] - \"%s\""
							sprintf(output, msg_txt(544), dstsd->away_message);
						else
							//"Away - \"%s\""
							sprintf(output, msg_txt(545), dstsd->away_message);
						clif_wis_message(fd, dstsd->status.name, output, strlen(output) + 1);
					} else { // Normal message
						clif_wis_message(dstsd->fd, sd->status.name, (char*)RFIFOP(fd,28), RFIFOW(fd,2) - 28);
						clif_wis_end(fd, 0); // 0: success to send wisper
					}
				}
			}
		}
	}

	return;
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_GMmessage(int fd, struct map_session_data *sd) {
// /b
	RFIFOHEAD(fd);
	if ((battle_config.atc_gmonly == 0 || pc_isGM(sd)) &&
	    (pc_isGM(sd) >= get_atcommand_level(AtCommand_Broadcast)))
		intif_GMmessage((char*)RFIFOP(fd,4), RFIFOW(fd,2)-4, 0);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_TakeItem(int fd, struct map_session_data *sd) {
	struct flooritem_data *fitem;
	int map_object_id;
	RFIFOHEAD(fd);

	map_object_id = RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);
	
	fitem = (struct flooritem_data*)map_id2bl(map_object_id);

	if (pc_isdead(sd)) {
		clif_clearchar_area(&sd->bl, 1);
		return;
	}

	if (fitem == NULL || fitem->bl.type != BL_ITEM || fitem->bl.m != sd->bl.m)
		return;
	
	if (clif_cant_act(sd))
		return;
	
	if(pc_iscloaking(sd) || pc_ischasewalk(sd)) //Disable cloaking/chasewalking characters from looting [Skotlex]
		return;
	if(sd->sc.count && (
		sd->sc.data[SC_TRICKDEAD].timer != -1 || //死んだふり
		sd->sc.data[SC_BLADESTOP].timer != -1 || //白刃取り
		sd->sc.data[SC_NOCHAT].timer!=-1 )	//会話禁止
	) {
		clif_additem(sd,0,0,6); // send fail packet! [Valaris]
		return;
	}

	pc_takeitem(sd, fitem);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_DropItem(int fd, struct map_session_data *sd) {
	int item_index, item_amount;
	RFIFOHEAD(fd);

	if (pc_isdead(sd)) {
		clif_clearchar_area(&sd->bl, 1);
		return;
	}

	if (clif_cant_act(sd))
		return;

	if (sd->sc.count && (
		sd->sc.data[SC_AUTOCOUNTER].timer != -1 || //オートカウンター
		sd->sc.data[SC_BLADESTOP].timer != -1//白刃取り
	))
		return;

	item_index = RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0])-2;
	item_amount = RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[1]);
	if (!pc_dropitem(sd, item_index, item_amount))
	//Because the client does not likes being ignored.
		clif_delitem(sd, item_index,0);

}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_UseItem(int fd, struct map_session_data *sd) {
	int n;
	RFIFOHEAD(fd);

	if (pc_isdead(sd)) {
		clif_clearchar_area(&sd->bl, 1);
		return;
	}

	if (sd->sc.opt1 > 0 && sd->sc.opt1 != OPT1_STONEWAIT)
		return;
	
	//This flag enables you to use items while in an NPC. [Skotlex]
	if (sd->npc_id) {
		if (sd->npc_id != sd->npc_item_flag)
			return;
	} else
	if (clif_trading(sd))
		return;
	
	if (sd->sc.count && (
		sd->sc.data[SC_TRICKDEAD].timer != -1 || //死んだふり
		sd->sc.data[SC_BLADESTOP].timer != -1 || //白刃取り
		sd->sc.data[SC_BERSERK].timer!=-1 ||	//バーサーク
		sd->sc.data[SC_NOCHAT].timer!=-1 ||
		sd->sc.data[SC_GRAVITATION].timer!=-1	//会話禁止
	))
		return;

	if (sd->invincible_timer != -1)
		pc_delinvincibletimer(sd);

	//Whether the item is used or not is irrelevant, the char ain't idle. [Skotlex]
	sd->idletime = last_tick;
	n = RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0])-2;
	
	if(n <0 || n >= MAX_INVENTORY)
		return;
	if (!pc_useitem(sd,n))
		clif_useitemack(sd,n,0,0); //Send an empty ack packet or the client gets stuck.
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_EquipItem(int fd,struct map_session_data *sd)
{
	int index;
	RFIFOHEAD(fd);

	if(pc_isdead(sd)) {
		clif_clearchar_area(&sd->bl,1);
		return;
	}
	index = RFIFOW(fd,2)-2; 
	if (index < 0 || index >= MAX_INVENTORY)
		return; //Out of bounds check.
	
	if(sd->npc_id) {
		if (sd->npc_id != sd->npc_item_flag)
			return;
	} else if (clif_cant_act(sd))
		return;
		
	if(sd->sc.data[SC_BLADESTOP].timer!=-1 || sd->sc.data[SC_BERSERK].timer!=-1 )
		return;

	if(!sd->status.inventory[index].identify) {		// 未鑑定
		clif_equipitemack(sd,index,0,0);	// fail
		return;
	}
	//ペット用装備であるかないか
	if(sd->inventory_data[index]) {
		if(sd->inventory_data[index]->type != 8){
			if(sd->inventory_data[index]->type == 10)
				RFIFOW(fd,4)=0x8000;	// 矢を無理やり装備できるように（−−；
			pc_equipitem(sd,index,RFIFOW(fd,4));
		} else
			pet_equipitem(sd,index);
	}
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_UnequipItem(int fd,struct map_session_data *sd)
{
	int index;
	RFIFOHEAD(fd);

	if(pc_isdead(sd)) {
		clif_clearchar_area(&sd->bl,1);
		return;
	}

	if (clif_cant_act(sd))
		return;

	index = RFIFOW(fd,2)-2;

	pc_unequipitem(sd,index,1);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_NpcClicked(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);

	if(pc_isdead(sd)) {
		clif_clearchar_area(&sd->bl,1);
		return;
	}

	if (clif_cant_act(sd))
		return;

	npc_click(sd,RFIFOL(fd,2));
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_NpcBuySellSelected(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
	if (sd->state.trading)
		return;
	npc_buysellsel(sd,RFIFOL(fd,2),RFIFOB(fd,6));
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_NpcBuyListSend(int fd,struct map_session_data *sd)
{
	int fail=0,n, i;
	unsigned short *item_list;
	unsigned char npc_ev[51];
	struct npc_data *nd;
	RFIFOHEAD(fd);

	n = (RFIFOW(fd,2)-4) /4;
	item_list = (unsigned short*)RFIFOP(fd,4);

	if (sd->state.trading|| !sd->npc_shopid)
		fail = 1;
	else{
		if((nd = ((struct npc_data *)map_id2bl(sd->npc_shopid))->master_nd)){
			sprintf(npc_ev, "%s::OnBuyItem", nd->exname);
			for(i=0;i<n;i++){
				setd_sub(NULL,sd, "@bought_nameid", i, (void *)((int)item_list[i*2+1]));
				setd_sub(NULL,sd, "@bought_quantity", i, (void *)((int)item_list[i*2]));
			}
			npc_event(sd, npc_ev, 0);
			fail = 0;
		}else{
			fail = npc_buylist(sd,n,item_list);
		}
	}

	sd->npc_shopid = 0; //Clear shop data.
	WFIFOHEAD(fd,packet_len_table[0xca]);
	WFIFOW(fd,0)=0xca;
	WFIFOB(fd,2)=fail;
	WFIFOSET(fd,packet_len_table[0xca]);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_NpcSellListSend(int fd,struct map_session_data *sd)
{
	int fail=0,n;
	unsigned short *item_list;
	RFIFOHEAD(fd);

	n = (RFIFOW(fd,2)-4) /4;
	item_list = (unsigned short*)RFIFOP(fd,4);

	if (sd->state.trading || !sd->npc_shopid)
		fail = 1;
	else
		fail = npc_selllist(sd,n,item_list);
	
	sd->npc_shopid = 0; //Clear shop data.

	WFIFOHEAD(fd,packet_len_table[0xcb]);
	WFIFOW(fd,0)=0xcb;
	WFIFOB(fd,2)=fail;
	WFIFOSET(fd,packet_len_table[0xcb]);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_CreateChatRoom(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
	if(battle_config.basic_skill_check == 0 || pc_checkskill(sd,NV_BASIC) >= 4){
		chat_createchat(sd,RFIFOW(fd,4),RFIFOB(fd,6),(char*)RFIFOP(fd,7),(char*)RFIFOP(fd,15),RFIFOW(fd,2)-15);
	} else
		clif_skill_fail(sd,1,0,3);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_ChatAddMember(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
	chat_joinchat(sd,RFIFOL(fd,2),(char*)RFIFOP(fd,6));
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_ChatRoomStatusChange(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
	chat_changechatstatus(sd,RFIFOW(fd,4),RFIFOB(fd,6),(char*)RFIFOP(fd,7),(char*)RFIFOP(fd,15),RFIFOW(fd,2)-15);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_ChangeChatOwner(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
	chat_changechatowner(sd,(char*)RFIFOP(fd,6));
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_KickFromChat(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
	chat_kickchat(sd,(char*)RFIFOP(fd,2));
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_ChatLeave(int fd,struct map_session_data *sd)
{
	chat_leavechat(sd);
}

/*==========================================
 * 取引要請を相手に送る
 *------------------------------------------
 */
void clif_parse_TradeRequest(int fd,struct map_session_data *sd)
{
	struct map_session_data *t_sd;
	
	RFIFOHEAD(fd);	
	t_sd = map_id2sd(RFIFOL(sd->fd,2));

	// @noask [LuzZza]
	if(t_sd && t_sd->state.noask) {
		// Your request has been rejected by autoreject option.
		clif_displaymessage(fd, msg_txt(392));
		return;
	}

	if(battle_config.basic_skill_check == 0 || pc_checkskill(sd,NV_BASIC) >= 1){
		trade_traderequest(sd,RFIFOL(sd->fd,2));
	} else
		clif_skill_fail(sd,1,0,0);
}

/*==========================================
 * 取引要請
 *------------------------------------------
 */
void clif_parse_TradeAck(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
	trade_tradeack(sd,RFIFOB(sd->fd,2));
}

/*==========================================
 * アイテム追加
 *------------------------------------------
 */
void clif_parse_TradeAddItem(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
	trade_tradeadditem(sd,RFIFOW(sd->fd,2),RFIFOL(sd->fd,4));
}

/*==========================================
 * アイテム追加完了(ok押し)
 *------------------------------------------
 */
void clif_parse_TradeOk(int fd,struct map_session_data *sd)
{
	trade_tradeok(sd);
}

/*==========================================
 * 取引キャンセル
 *------------------------------------------
 */
void clif_parse_TradeCancel(int fd,struct map_session_data *sd)
{
	trade_tradecancel(sd);
}

/*==========================================
 * 取引許諾(trade押し)
 *------------------------------------------
 */
void clif_parse_TradeCommit(int fd,struct map_session_data *sd)
{
	trade_tradecommit(sd);
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_StopAttack(int fd,struct map_session_data *sd)
{
	pc_stop_attack(sd);
}

/*==========================================
 * カートへアイテムを移す
 *------------------------------------------
 */
void clif_parse_PutItemToCart(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);

	if (clif_trading(sd))
		return;
	pc_putitemtocart(sd,RFIFOW(fd,2)-2,RFIFOL(fd,4));
}
/*==========================================
 * カートからアイテムを出す
 *------------------------------------------
 */
void clif_parse_GetItemFromCart(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
	pc_getitemfromcart(sd,RFIFOW(fd,2)-2,RFIFOL(fd,4));
}

/*==========================================
 * 付属品(鷹,ペコ,カート)をはずす
 *------------------------------------------
 */
void clif_parse_RemoveOption(int fd,struct map_session_data *sd)
{
	pc_setoption(sd,0);
}

/*==========================================
 * チェンジカート
 *------------------------------------------
 */
void clif_parse_ChangeCart(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
	pc_setcart(sd,RFIFOW(fd,2));
}

/*==========================================
 * ステータスアップ
 *------------------------------------------
 */
void clif_parse_StatusUp(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
	pc_statusup(sd,RFIFOW(fd,2));
}

/*==========================================
 * スキルレベルアップ
 *------------------------------------------
 */
void clif_parse_SkillUp(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
	pc_skillup(sd,RFIFOW(fd,2));
}

/*==========================================
 * スキル使用（ID指定）
 *------------------------------------------
 */
void clif_parse_UseSkillToId(int fd, struct map_session_data *sd) {
	int skillnum, skilllv, lv, target_id;
	unsigned int tick = gettick();
	RFIFOHEAD(fd);

	if (clif_cant_act(sd))
		return;
	if (pc_issit(sd))
		return;

	skilllv = RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);
	if (skilllv < 1) skilllv = 1; //No clue, I have seen the client do this with guild skills :/ [Skotlex]
	skillnum = RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[1]);
	target_id = RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[2]);

	//Whether skill fails or not is irrelevant, the char ain't idle. [Skotlex]
	sd->idletime = last_tick;
	
	if (skillnotok(skillnum, sd))
		return;

	if (sd->bl.id != target_id &&
		!sd->state.skill_flag &&
		skill_get_inf(skillnum)&INF_SELF_SKILL)
		target_id = sd->bl.id; //What good is it to mess up the target in self skills? Wished I knew... [Skotlex]
	
	if (sd->ud.skilltimer != -1) {
		if (skillnum != SA_CASTCANCEL)
			return;
	} else if (DIFF_TICK(tick, sd->ud.canact_tick) < 0 &&
		// allow monk combos to ignore this delay [celest]
		!(sd->sc.count && sd->sc.data[SC_COMBO].timer!=-1 &&
		(skillnum == MO_EXTREMITYFIST ||
		skillnum == MO_CHAINCOMBO ||
		skillnum == MO_COMBOFINISH ||
		skillnum == CH_PALMSTRIKE ||
		skillnum == CH_TIGERFIST ||
		skillnum == CH_CHAINCRUSH))) {
		clif_skill_fail(sd, skillnum, 4, 0);
		return;
	}

	if(sd->sc.option&OPTION_WEDDING || sd->vd.class_ == JOB_XMAS)
		return;
	
	if (sd->invincible_timer != -1)
		pc_delinvincibletimer(sd);
	
	if(target_id<0 && -target_id == sd->bl.id) // for disguises [Valaris]
		target_id = sd->bl.id;
		
	if (sd->skillitem >= 0 && sd->skillitem == skillnum) {
		if (skilllv != sd->skillitemlv)
			skilllv = sd->skillitemlv;
		unit_skilluse_id(&sd->bl, target_id, skillnum, skilllv);
	} else {
		sd->skillitem = sd->skillitemlv = -1;
		if (skillnum == MO_EXTREMITYFIST) {
			if ((sd->sc.data[SC_COMBO].timer == -1 ||
				(sd->sc.data[SC_COMBO].val1 != MO_COMBOFINISH &&
				sd->sc.data[SC_COMBO].val1 != CH_TIGERFIST &&
				sd->sc.data[SC_COMBO].val1 != CH_CHAINCRUSH))) {
				if (!sd->state.skill_flag ) {
					sd->state.skill_flag = 1;
					clif_skillinfo(sd, MO_EXTREMITYFIST, INF_ATTACK_SKILL, -1);
					return;
				} else if (sd->bl.id == target_id) {
					clif_skillinfo(sd, MO_EXTREMITYFIST, INF_ATTACK_SKILL, -1);
					return;
				}
			}
		}
		if (skillnum == TK_JUMPKICK) {
			if (sd->sc.data[SC_COMBO].timer == -1 ||
				sd->sc.data[SC_COMBO].val1 != TK_JUMPKICK) {
				if (!sd->state.skill_flag ) {
					sd->state.skill_flag = 1;
					clif_skillinfo(sd, TK_JUMPKICK, INF_ATTACK_SKILL, -1);
					return;
				} else if (sd->bl.id == target_id) {
					clif_skillinfo(sd, TK_JUMPKICK, INF_ATTACK_SKILL, -1);
					return;
				}
			}
		}

		if (skillnum >= GD_SKILLBASE && sd->state.gmaster_flag)
			skilllv = guild_checkskill(sd->state.gmaster_flag, skillnum);
		
		if ((lv = pc_checkskill(sd, skillnum)) > 0) {
			if (skilllv > lv)
				skilllv = lv;
			unit_skilluse_id(&sd->bl, target_id, skillnum, skilllv);
			if (sd->state.skill_flag)
				sd->state.skill_flag = 0;
		}
	}
}

/*==========================================
 * スキル使用（場所指定）
 *------------------------------------------
 */
void clif_parse_UseSkillToPosSub(int fd, struct map_session_data *sd, int skilllv, int skillnum, int x, int y, int skillmoreinfo)
{
	int lv;
	unsigned int tick = gettick();
	RFIFOHEAD(fd);

	//Whether skill fails or not is irrelevant, the char ain't idle. [Skotlex]
	sd->idletime = last_tick;

	if (skillnotok(skillnum, sd))
		return;

	if (skillmoreinfo != -1) {
		if (pc_issit(sd)) {
			clif_skill_fail(sd, skillnum, 0, 0);
			return;
		}
		memcpy(talkie_mes, RFIFOP(fd,skillmoreinfo), MESSAGE_SIZE);
		talkie_mes[MESSAGE_SIZE-1] = '\0'; //Overflow protection [Skotlex]
	}

	if (sd->ud.skilltimer != -1)
		return;
	if (DIFF_TICK(tick, sd->ud.canact_tick) < 0)
	{
		clif_skill_fail(sd, skillnum, 4, 0);
		return;
	}

	if(sd->sc.option&OPTION_WEDDING || sd->vd.class_ == JOB_XMAS)
		return;
	
	if (sd->invincible_timer != -1)
		pc_delinvincibletimer(sd);
	if (sd->skillitem >= 0 && sd->skillitem == skillnum) {
		if (skilllv != sd->skillitemlv)
			skilllv = sd->skillitemlv;
		unit_skilluse_pos(&sd->bl, x, y, skillnum, skilllv);
	} else {
		sd->skillitem = sd->skillitemlv = -1;
		if ((lv = pc_checkskill(sd, skillnum)) > 0) {
			if (skilllv > lv)
				skilllv = lv;
			unit_skilluse_pos(&sd->bl, x, y, skillnum,skilllv);
		}
	}
}


void clif_parse_UseSkillToPos(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);

	if (clif_cant_act(sd))
		return;
	if (pc_issit(sd))
		return;

	clif_parse_UseSkillToPosSub(fd, sd,
		RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]), //skill lv
		RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[1]), //skill num
		RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[2]), //pos x
		RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[3]), //pos y
		-1	//Skill more info.
	);
}

void clif_parse_UseSkillToPosMoreInfo(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);

	if (clif_cant_act(sd))
		return;
	if (pc_issit(sd))
		return;
	
	clif_parse_UseSkillToPosSub(fd, sd,
		RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]), //Skill lv
		RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[1]), //Skill num
		RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[2]), //pos x
		RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[3]), //pos y
		packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[4] //skill more info
	);
}
/*==========================================
 * スキル使用（map指定）
 *------------------------------------------
 */
void clif_parse_UseSkillMap(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);

	if (clif_cant_act(sd))
		return;

	if(sd->sc.option&OPTION_WEDDING || sd->vd.class_ == JOB_XMAS)
		return;
	
	if(sd->invincible_timer != -1)
		pc_delinvincibletimer(sd);

	skill_castend_map(sd,RFIFOW(fd,2),(char*)RFIFOP(fd,4));
}
/*==========================================
 * メモ要求
 *------------------------------------------
 */
void clif_parse_RequestMemo(int fd,struct map_session_data *sd)
{
	if (!pc_isdead(sd))
		pc_memo(sd,-1);
}
/*==========================================
 * アイテム合成
 *------------------------------------------
 */
void clif_parse_ProduceMix(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);

	if (sd->menuskill_id !=	AM_PHARMACY)
		return;

	if (clif_trading(sd)) {
		//Make it fail to avoid shop exploits where you sell something different than you see.
		clif_skill_fail(sd,sd->ud.skillid,0,0);
		sd->menuskill_lv = sd->menuskill_id = 0;
		return;
	}
	skill_produce_mix(sd,0,RFIFOW(fd,2),RFIFOW(fd,4),RFIFOW(fd,6),RFIFOW(fd,8), 1);
	sd->menuskill_lv = sd->menuskill_id = 0;
}
/*==========================================
 * 武器修理
 *------------------------------------------
 */
void clif_parse_RepairItem(int fd, struct map_session_data *sd)
{
	RFIFOHEAD(fd);

	if (sd->menuskill_id != BS_REPAIRWEAPON)
		return;
	if (clif_trading(sd)) {
		//Make it fail to avoid shop exploits where you sell something different than you see.
		clif_skill_fail(sd,sd->ud.skillid,0,0);
		sd->menuskill_lv = sd->menuskill_id = 0;
		return;
	}
	skill_repairweapon(sd,RFIFOW(fd,2));
	sd->menuskill_lv = sd->menuskill_id = 0;
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_WeaponRefine(int fd, struct map_session_data *sd) {
	int idx;
	RFIFOHEAD(fd);

	if (sd->menuskill_id != WS_WEAPONREFINE) //Packet exploit?
		return;
	if (clif_trading(sd)) {
		//Make it fail to avoid shop exploits where you sell something different than you see.
		clif_skill_fail(sd,sd->ud.skillid,0,0);
		sd->menuskill_lv = sd->menuskill_id = 0;
		return;
	}
	idx = RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);
	skill_weaponrefine(sd, idx-2);
	sd->menuskill_lv = sd->menuskill_id = 0;
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_NpcSelectMenu(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);

	sd->npc_menu=RFIFOB(fd,6);
	npc_scriptcont(sd,RFIFOL(fd,2));
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_NpcNextClicked(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
#ifdef __WIN32
	//For some extraordinarily eerie reason that noone has figured out yet, 
	//windows native compiles NEED this nullpo_retv or the function is not found!
	nullpo_retv(sd);
#endif
	npc_scriptcont(sd,RFIFOL(fd,2));
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_NpcAmountInput(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
#define RFIFOL_(fd,pos) (*(int*)(session[fd]->rdata+session[fd]->rdata_pos+(pos)))
	//Input Value overflow Exploit FIX
	sd->npc_amount=RFIFOL_(fd,6); //fixed by Lupus. npc_amount is (int) but was RFIFOL changing it to (unsigned int)
#undef RFIFOL_

	npc_scriptcont(sd,RFIFOL(fd,2));
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_NpcStringInput(int fd,struct map_session_data *sd)
{
	unsigned short message_len = RFIFOW(fd,2)-7;
	RFIFOHEAD(fd);

	if(message_len >= sizeof(sd->npc_str)){
		ShowWarning("clif: input string too long !\n");
		message_len = sizeof(sd->npc_str);
	} else {
		message_len += 1; // Null character
	}

	// Exploit prevention if crafted packets (without null) is being sent. [Lance]
	memcpy(sd->npc_str,RFIFOP(fd,8),message_len); 
	sd->npc_str[message_len-1]=0;

	npc_scriptcont(sd,RFIFOL(fd,4));
}

/*==========================================
 *
 *------------------------------------------
 */
void clif_parse_NpcCloseClicked(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
	if (sd->npc_id) //Avoid parsing anything when the script was done with. [Skotlex]
		npc_scriptcont(sd,RFIFOL(fd,2));
}

/*==========================================
 * アイテム鑑定
 *------------------------------------------
 */
void clif_parse_ItemIdentify(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
	if (sd->menuskill_id != MC_IDENTIFY)
		return;
	skill_identify(sd,RFIFOW(fd,2)-2);
	sd->menuskill_lv = sd->menuskill_id = 0;
}
/*==========================================
 * 矢作成
 *------------------------------------------
 */
void clif_parse_SelectArrow(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
	if (sd->menuskill_id != AC_MAKINGARROW)
		return;
	if (clif_trading(sd)) {
	//Make it fail to avoid shop exploits where you sell something different than you see.
		clif_skill_fail(sd,sd->ud.skillid,0,0);
		sd->menuskill_lv = sd->menuskill_id = 0;
		return;
	}
	skill_arrow_create(sd,RFIFOW(fd,2));
	sd->menuskill_lv = sd->menuskill_id = 0;
}
/*==========================================
 * オートスペル受信
 *------------------------------------------
 */
void clif_parse_AutoSpell(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
	if (sd->menuskill_id != SA_AUTOSPELL)
		return;
	skill_autospell(sd,RFIFOW(fd,2));
	sd->menuskill_lv = sd->menuskill_id = 0;
}
/*==========================================
 * カード使用
 *------------------------------------------
 */
void clif_parse_UseCard(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
	if (sd->state.trading!= 0)
		return;
	clif_use_card(sd,RFIFOW(fd,2)-2);
}
/*==========================================
 * カード挿入装備選択
 *------------------------------------------
 */
void clif_parse_InsertCard(int fd,struct map_session_data *sd)
{
	RFIFOHEAD(fd);
	if (sd->state.trading!= 0)
		return;
	pc_insert_card(sd,RFIFOW(fd,2)-2,RFIFOW(fd,4)-2);
}

/*==========================================
 * 0193 キャラID名前引き
 *------------------------------------------
 */
void clif_parse_SolveCharName(int fd, struct map_session_data *sd) {
	int char_id;
	RFIFOHEAD(fd);

	char_id = RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0]);
	clif_solved_charname(sd, char_id);
}

/*==========================================
 * 0197 /resetskill /resetstate
 *------------------------------------------
 */
void clif_parse_ResetChar(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);
	if ((battle_config.atc_gmonly == 0 || pc_isGM(sd)) &&
		pc_isGM(sd) >= get_atcommand_level(AtCommand_ResetState)) {
		switch(RFIFOW(fd,2)){
		case 0:
			pc_resetstate(sd);
			break;
		case 1:
			pc_resetskill(sd,1);
			break;
		}
	}
}

/*==========================================
 * 019c /lb等
 *------------------------------------------
 */
void clif_parse_LGMmessage(int fd, struct map_session_data *sd) {
	unsigned char buf[512];
	RFIFOHEAD(fd);

	if ((battle_config.atc_gmonly == 0 || pc_isGM(sd)) &&
	    (pc_isGM(sd) >= get_atcommand_level(AtCommand_LocalBroadcast))) {
		WBUFW(buf,0) = 0x9a;
		WBUFW(buf,2) = RFIFOW(fd,2);
		memcpy(WBUFP(buf,4), RFIFOP(fd,4), RFIFOW(fd,2) - 4);
		clif_send(buf, RFIFOW(fd,2), &sd->bl, ALL_SAMEMAP);
	}
}

/*==========================================
 * カプラ倉庫へ入れる
 *------------------------------------------
 */
void clif_parse_MoveToKafra(int fd, struct map_session_data *sd) {
	int item_index, item_amount;
	RFIFOHEAD(fd);

	if (clif_trading(sd))
		return;
	
	item_index = RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0])-2;
	item_amount = RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[1]);
	if (item_index < 0 || item_index >= MAX_INVENTORY || item_amount < 1)
		return;

	if (sd->state.storage_flag == 1)
		storage_storageadd(sd, item_index, item_amount);
	else if (sd->state.storage_flag == 2)
		storage_guild_storageadd(sd, item_index, item_amount);
}

/*==========================================
 * カプラ倉庫から出す
 *------------------------------------------
 */
void clif_parse_MoveFromKafra(int fd,struct map_session_data *sd) {
	int item_index, item_amount;
	RFIFOHEAD(fd);

	item_index = RFIFOW(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[0])-1;
	item_amount = RFIFOL(fd,packet_db[sd->packet_ver][RFIFOW(fd,0)].pos[1]);

	if (sd->state.storage_flag == 1)
		storage_storageget(sd, item_index, item_amount);
	else if(sd->state.storage_flag == 2)
		storage_guild_storageget(sd, item_index, item_amount);
}

/*==========================================
 * カプラ倉庫へカートから入れる
 *------------------------------------------
 */
void clif_parse_MoveToKafraFromCart(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);

	if(sd->vender_id)	
		return;

	if (sd->state.storage_flag == 1)
		storage_storageaddfromcart(sd, RFIFOW(fd,2) - 2, RFIFOL(fd,4));
	else	if (sd->state.storage_flag == 2)
		storage_guild_storageaddfromcart(sd, RFIFOW(fd,2) - 2, RFIFOL(fd,4));
}

/*==========================================
 * カプラ倉庫から出す
 *------------------------------------------
 */
void clif_parse_MoveFromKafraToCart(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);

	if (sd->vender_id)
		return;
	if (sd->state.storage_flag == 1)
		storage_storagegettocart(sd, RFIFOW(fd,2)-1, RFIFOL(fd,4));
	else if (sd->state.storage_flag == 2)
		storage_guild_storagegettocart(sd, RFIFOW(fd,2)-1, RFIFOL(fd,4));
}

/*==========================================
 * カプラ倉庫を閉じる
 *------------------------------------------
 */
void clif_parse_CloseKafra(int fd, struct map_session_data *sd) {
	if (sd->state.storage_flag == 1)
		storage_storageclose(sd);
	else if (sd->state.storage_flag == 2)
		storage_guild_storageclose(sd);
}

/*==========================================
 * パーティを作る
 *------------------------------------------
 */
void clif_parse_CreateParty(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);
	if (battle_config.basic_skill_check == 0 || pc_checkskill(sd,NV_BASIC) >= 7) {
		party_create(sd,(char*)RFIFOP(fd,2),0,0);
	} else
		clif_skill_fail(sd,1,0,4);
}

/*==========================================
 * パーティを作る
 *------------------------------------------
 */
void clif_parse_CreateParty2(int fd, struct map_session_data *sd) {
	if (battle_config.basic_skill_check == 0 || pc_checkskill(sd,NV_BASIC) >= 7){
		RFIFOHEAD(fd);
		party_create(sd,(char*)RFIFOP(fd,2),RFIFOB(fd,26),RFIFOB(fd,27));
	} else
		clif_skill_fail(sd,1,0,4);
}

/*==========================================
 * パーティに勧誘
 *------------------------------------------
 */
void clif_parse_PartyInvite(int fd, struct map_session_data *sd) {
	
	struct map_session_data *t_sd;
	
	RFIFOHEAD(fd);	
	t_sd = map_id2sd(RFIFOL(sd->fd,2));

	// @noask [LuzZza]
	if(t_sd && t_sd->state.noask) {
		// Your request has been rejected by autoreject option.
		clif_displaymessage(fd, msg_txt(392));
		return;
	}
	
	party_invite(sd, RFIFOL(fd,2));
}

/*==========================================
 * パーティ勧誘返答
 *------------------------------------------
 */
void clif_parse_ReplyPartyInvite(int fd,struct map_session_data *sd) {
	RFIFOHEAD(fd);
	if(battle_config.basic_skill_check == 0 || pc_checkskill(sd,NV_BASIC) >= 5){
		party_reply_invite(sd,RFIFOL(fd,2),RFIFOL(fd,6));
	} else {
		party_reply_invite(sd,RFIFOL(fd,2),-1);
		clif_skill_fail(sd,1,0,4);
	}
}

/*==========================================
 * パーティ脱退要求
 *------------------------------------------
 */
void clif_parse_LeaveParty(int fd, struct map_session_data *sd) {
	party_leave(sd);
}

/*==========================================
 * パーティ除名要求
 *------------------------------------------
 */
void clif_parse_RemovePartyMember(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);
	party_removemember(sd,RFIFOL(fd,2),(char*)RFIFOP(fd,6));
}

/*==========================================
 * パーティ設定変更要求
 *------------------------------------------
 */
void clif_parse_PartyChangeOption(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);
	party_changeoption(sd, RFIFOW(fd,2), RFIFOW(fd,4));
}

/*==========================================
 * パーティメッセージ送信要求
 *------------------------------------------
 */
void clif_parse_PartyMessage(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);

	if (is_charcommand(fd, sd, (char*)RFIFOP(fd,4), 0) != CharCommand_None ||
		is_atcommand(fd, sd, (char*)RFIFOP(fd,4), 0) != AtCommand_None)
		return;
	if	(sd->sc.count && (
			sd->sc.data[SC_BERSERK].timer!=-1 ||	//バーサーク時は会話も不可
			sd->sc.data[SC_NOCHAT].timer!=-1		//チャット禁止
		))
		return;

	party_send_message(sd, (char*)RFIFOP(fd,4), RFIFOW(fd,2)-4);
}

/*==========================================
 * 露店閉鎖
 *------------------------------------------
 */
void clif_parse_CloseVending(int fd, struct map_session_data *sd) {
	vending_closevending(sd);
}

/*==========================================
 * 露店アイテムリスト要求
 *------------------------------------------
 */
void clif_parse_VendingListReq(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);

	vending_vendinglistreq(sd,RFIFOL(fd,2));
	if(sd->npc_id)
		npc_event_dequeue(sd);
}

/*==========================================
 * 露店アイテム購入
 *------------------------------------------
 */
void clif_parse_PurchaseReq(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);
	vending_purchasereq(sd, RFIFOW(fd,2), RFIFOL(fd,4), RFIFOP(fd,8));
}

/*==========================================
 * 露店開設
 *------------------------------------------
 */
void clif_parse_OpenVending(int fd,struct map_session_data *sd) {
	RFIFOHEAD(fd);
	if (clif_trading(sd))
		return;
	vending_openvending(sd, RFIFOW(fd,2), (char*)RFIFOP(fd,4), RFIFOB(fd,84), RFIFOP(fd,85));
}

/*==========================================
 * ギルドを作る
 *------------------------------------------
 */
void clif_parse_CreateGuild(int fd,struct map_session_data *sd) {
	RFIFOHEAD(fd);
	guild_create(sd, (char*)RFIFOP(fd,6));
}

/*==========================================
 * ギルドマスターかどうか確認
 *------------------------------------------
 */
void clif_parse_GuildCheckMaster(int fd, struct map_session_data *sd) {
	clif_guild_masterormember(sd);
}

/*==========================================
 * ギルド情報要求
 *------------------------------------------
 */
void clif_parse_GuildRequestInfo(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);
	switch(RFIFOL(fd,2)){
	case 0:	// ギルド基本情報、同盟敵対情報
		clif_guild_basicinfo(sd);
		clif_guild_allianceinfo(sd);
		break;
	case 1:	// メンバーリスト、役職名リスト
		clif_guild_positionnamelist(sd);
		clif_guild_memberlist(sd);
		break;
	case 2:	// 役職名リスト、役職情報リスト
		clif_guild_positionnamelist(sd);
		clif_guild_positioninfolist(sd);
		break;
	case 3:	// スキルリスト
		clif_guild_skillinfo(sd);
		break;
	case 4:	// 追放リスト
		clif_guild_explusionlist(sd);
		break;
	default:
		if (battle_config.error_log)
			ShowError("clif: guild request info: unknown type %d\n", RFIFOL(fd,2));
		break;
	}
}

/*==========================================
 * ギルド役職変更
 *------------------------------------------
 */
void clif_parse_GuildChangePositionInfo(int fd, struct map_session_data *sd) {
	int i;
	RFIFOHEAD(fd);

	for(i = 4; i < RFIFOW(fd,2); i += 40 ){
		guild_change_position(sd, RFIFOL(fd,i), RFIFOL(fd,i+4), RFIFOL(fd,i+12), (char*)RFIFOP(fd,i+16));
	}
}

/*==========================================
 * ギルドメンバ役職変更
 *------------------------------------------
 */
void clif_parse_GuildChangeMemberPosition(int fd, struct map_session_data *sd) {
	int i;
	RFIFOHEAD(fd);

	for(i=4;i<RFIFOW(fd,2);i+=12){
		guild_change_memberposition(sd->status.guild_id,
			RFIFOL(fd,i),RFIFOL(fd,i+4),RFIFOL(fd,i+8));
	}
}

/*==========================================
 * ギルドエンブレム要求
 *------------------------------------------
 */
void clif_parse_GuildRequestEmblem(int fd,struct map_session_data *sd) {
	struct guild *g;
	RFIFOHEAD(fd);
	g=guild_search(RFIFOL(fd,2));
	if(g!=NULL)
		clif_guild_emblem(sd,g);
}

/*==========================================
 * ギルドエンブレム変更
 *------------------------------------------
 */
void clif_parse_GuildChangeEmblem(int fd,struct map_session_data *sd) {
	RFIFOHEAD(fd);
	guild_change_emblem(sd,RFIFOW(fd,2)-4,(char*)RFIFOP(fd,4));
}

/*==========================================
 * ギルド告知変更
 *------------------------------------------
 */
void clif_parse_GuildChangeNotice(int fd,struct map_session_data *sd) {
	RFIFOHEAD(fd);
	guild_change_notice(sd,RFIFOL(fd,2),(char*)RFIFOP(fd,6),(char*)RFIFOP(fd,66));
}

/*==========================================
 * ギルド勧誘
 *------------------------------------------
 */
void clif_parse_GuildInvite(int fd,struct map_session_data *sd) {

	struct map_session_data *t_sd;
	
	RFIFOHEAD(fd);	
	t_sd = map_id2sd(RFIFOL(sd->fd,2));

	// @noask [LuzZza]
	if(t_sd && t_sd->state.noask) {
		// Your request has been rejected by autoreject option.
		clif_displaymessage(fd, msg_txt(392));
		return;
	}

	guild_invite(sd,RFIFOL(fd,2));
}

/*==========================================
 * ギルド勧誘返信
 *------------------------------------------
 */
void clif_parse_GuildReplyInvite(int fd,struct map_session_data *sd) {
	RFIFOHEAD(fd);
	guild_reply_invite(sd,RFIFOL(fd,2),RFIFOB(fd,6));
}

/*==========================================
 * ギルド脱退
 *------------------------------------------
 */
void clif_parse_GuildLeave(int fd,struct map_session_data *sd) {
	RFIFOHEAD(fd);
	guild_leave(sd,RFIFOL(fd,2),RFIFOL(fd,6),RFIFOL(fd,10),(char*)RFIFOP(fd,14));
}

/*==========================================
 * ギルド追放
 *------------------------------------------
 */
void clif_parse_GuildExplusion(int fd,struct map_session_data *sd) {
	RFIFOHEAD(fd);
	guild_explusion(sd,RFIFOL(fd,2),RFIFOL(fd,6),RFIFOL(fd,10),(char*)RFIFOP(fd,14));
}

/*==========================================
 * ギルド会話
 *------------------------------------------
 */
void clif_parse_GuildMessage(int fd,struct map_session_data *sd) {
	RFIFOHEAD(fd);

	if (is_charcommand(fd, sd, (char*)RFIFOP(fd, 4), 0) != CharCommand_None ||
		is_atcommand(fd, sd, (char*)RFIFOP(fd, 4), 0) != AtCommand_None)
		return;
	if (sd->sc.count && (
		sd->sc.data[SC_BERSERK].timer!=-1 ||	//バーサーク時は会話も不可
		sd->sc.data[SC_NOCHAT].timer!=-1		//チャット禁止
	))
		return;

	guild_send_message(sd, (char*)RFIFOP(fd,4), RFIFOW(fd,2)-4);
}

/*==========================================
 * ギルド同盟要求
 *------------------------------------------
 */
void clif_parse_GuildRequestAlliance(int fd, struct map_session_data *sd) {
	
	struct map_session_data *t_sd;
	
	RFIFOHEAD(fd);	
	t_sd = map_id2sd(RFIFOL(sd->fd,2));

	// @noask [LuzZza]
	if(t_sd && t_sd->state.noask) {
		// Your request has been rejected by autoreject option.
		clif_displaymessage(fd, msg_txt(392));
		return;
	}
	
	guild_reqalliance(sd,RFIFOL(fd,2));
}

/*==========================================
 * ギルド同盟要求返信
 *------------------------------------------
 */
void clif_parse_GuildReplyAlliance(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);
	guild_reply_reqalliance(sd,RFIFOL(fd,2),RFIFOL(fd,6));
}

/*==========================================
 * ギルド関係解消
 *------------------------------------------
 */
void clif_parse_GuildDelAlliance(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);
	guild_delalliance(sd,RFIFOL(fd,2),RFIFOL(fd,6));
}

/*==========================================
 * ギルド敵対
 *------------------------------------------
 */
void clif_parse_GuildOpposition(int fd, struct map_session_data *sd) {
	
	struct map_session_data *t_sd;
	
	RFIFOHEAD(fd);	
	t_sd = map_id2sd(RFIFOL(sd->fd,2));

	// @noask [LuzZza]
	if(t_sd && t_sd->state.noask) {
		// Your request has been rejected by autoreject option.
		clif_displaymessage(fd, msg_txt(392));
		return;
	}
	
	guild_opposition(sd,RFIFOL(fd,2));
}

/*==========================================
 * ギルド解散
 *------------------------------------------
 */
void clif_parse_GuildBreak(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);
	guild_break(sd,(char*)RFIFOP(fd,2));
}

// pet
void clif_parse_PetMenu(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);
	pet_menu(sd,RFIFOB(fd,2));
}

void clif_parse_CatchPet(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);
	pet_catch_process2(sd,RFIFOL(fd,2));
}

void clif_parse_SelectEgg(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);
	if (sd->menuskill_id != SA_TAMINGMONSTER || sd->menuskill_lv != -1)
		return;
	pet_select_egg(sd,RFIFOW(fd,2)-2);
	sd->menuskill_lv = sd->menuskill_id = 0;
}

void clif_parse_SendEmotion(int fd, struct map_session_data *sd) {
	if(sd->pd) {
		RFIFOHEAD(fd);
		clif_pet_emotion(sd->pd,RFIFOL(fd,2));
	}
}

void clif_parse_ChangePetName(int fd, struct map_session_data *sd) {
	RFIFOHEAD(fd);
	pet_change_name(sd,(char*)RFIFOP(fd,2));
}

// Kick (right click menu for GM "(name) force to quit")
void clif_parse_GMKick(int fd, struct map_session_data *sd) {
	struct block_list *target;
	int tid;

	RFIFOHEAD(fd);
	tid = RFIFOL(fd,2);

	if ((battle_config.atc_gmonly == 0 || pc_isGM(sd)) &&
	    (pc_isGM(sd) >= get_atcommand_level(AtCommand_Kick))) {
		target = map_id2bl(tid);
		if (target) {
			if (target->type == BL_PC) {
				struct map_session_data *tsd = (struct map_session_data *)target;
				if (pc_isGM(sd) > pc_isGM(tsd))
					clif_GM_kick(sd, tsd, 1);
				else
					clif_GM_kickack(sd, 0);
			} else if (target->type == BL_MOB) {
				struct mob_data *md = (struct mob_data *)target;
				mob_damage(&sd->bl, md, md->hp, 2);
			} else
				clif_GM_kickack(sd, 0);
		} else
			clif_GM_kickack(sd, 0);
	}
}

/*==========================================
 * /shift
 *------------------------------------------
 */
void clif_parse_Shift(int fd, struct map_session_data *sd) {	// Rewriten by [Yor]
	char player_name[NAME_LENGTH+1];

	memset(player_name, '\0', sizeof(player_name));

	if ((battle_config.atc_gmonly == 0 || pc_isGM(sd)) &&
	    (pc_isGM(sd) >= get_atcommand_level(AtCommand_JumpTo))) {
          RFIFOHEAD(fd);
          memcpy(player_name, RFIFOP(fd,2), NAME_LENGTH);
          atcommand_jumpto(fd, sd, "@jumpto", player_name); // as @jumpto
	}

	return;
}

/*==========================================
 * /recall
 *------------------------------------------
 */
void clif_parse_Recall(int fd, struct map_session_data *sd) {	// Added by RoVeRT
	char player_name[25];

	memset(player_name, '\0', sizeof(player_name));

	if ((battle_config.atc_gmonly == 0 || pc_isGM(sd)) &&
	    (pc_isGM(sd) >= get_atcommand_level(AtCommand_Recall))) {
		RFIFOHEAD(fd);
		memcpy(player_name, RFIFOP(fd,2), NAME_LENGTH);
		atcommand_recall(fd, sd, "@recall", player_name); // as @recall
	}

	return;
}

/*==========================================
 * /monster /item rewriten by [Yor]
 *------------------------------------------
 */
void clif_parse_GM_Monster_Item(int fd, struct map_session_data *sd) {
	char monster_item_name[NAME_LENGTH+10]; //Additional space is for logging, eg: "@monster Poring"
	int level;

	memset(monster_item_name, '\0', sizeof(monster_item_name));

	if (battle_config.atc_gmonly == 0 || pc_isGM(sd)) {
		RFIFOHEAD(fd);
		memcpy(monster_item_name, RFIFOP(fd,2), NAME_LENGTH);

		if (mobdb_searchname(monster_item_name) != 0) {
			if (pc_isGM(sd) >= (level =get_atcommand_level(AtCommand_Spawn)))	// changed from AtCommand_Monster for Skots [Reddozen]
			{
				atcommand_monster(fd, sd, "@spawn", monster_item_name); // as @spawn
				if(log_config.gm && level >= log_config.gm)
				{	//Log action. [Skotlex]
					snprintf(monster_item_name, sizeof(monster_item_name)-1, "@spawn %s", RFIFOP(fd,2));
					log_atcommand(sd, monster_item_name);
				}
			}
		} else if (itemdb_searchname(monster_item_name) != NULL) {
			if (pc_isGM(sd) >= (level = get_atcommand_level(AtCommand_Item)))
			{
				atcommand_item(fd, sd, "@item", monster_item_name); // as @item
				if(log_config.gm && level >= log_config.gm)
				{	//Log action. [Skotlex]
					snprintf(monster_item_name, sizeof(monster_item_name)-1, "@item %s", RFIFOP(fd,2));
					log_atcommand(sd, monster_item_name);
				}
			}
		}
	}
}

void clif_parse_GMHide(int fd, struct map_session_data *sd) {	// Modified by [Yor]
	if ((battle_config.atc_gmonly == 0 || pc_isGM(sd)) &&
	    (pc_isGM(sd) >= get_atcommand_level(AtCommand_Hide))) {
		if (sd->sc.option & OPTION_INVISIBLE) {
			sd->sc.option &= ~OPTION_INVISIBLE;
			if (sd->disguise)
				status_set_viewdata(&sd->bl, sd->disguise);
			else
				status_set_viewdata(&sd->bl, sd->status.class_);
			clif_displaymessage(fd, "Invisible: Off.");
		} else {
			sd->sc.option |= OPTION_INVISIBLE;
			//Experimental hidden mode, changes your view class to invisible [Skotlex]
			sd->vd.class_ = INVISIBLE_CLASS;
			clif_displaymessage(fd, "Invisible: On.");
		}
		clif_changeoption(&sd->bl);
	}
}

/*==========================================
 * GMによるチャット禁止時間付与
 *------------------------------------------
 */
void clif_parse_GMReqNoChat(int fd,struct map_session_data *sd)
{
	int type, limit, level;
	struct block_list *bl;
	struct map_session_data *dstsd;

	if(!battle_config.muting_players) {
		clif_displaymessage(fd, "Muting is disabled.");
		return;
	}

	RFIFOHEAD(fd);
	bl = map_id2bl(RFIFOL(fd,2));
	if (!bl || bl->type != BL_PC)
		return;
	nullpo_retv(dstsd =(struct map_session_data *)bl);

	type = RFIFOB(fd,6);
	limit = RFIFOW(fd,7);
	if (type == 0)
		limit = 0 - limit;

	//Temporarily disable chars from muting themselves due to the mysterious "DON'T USE BOT!" message. [Skotlex]
	//Also, if type is 2 and the ids don't match, this is a crafted hacked packet!
	//So for now, type 2is just totally disabled.
	if (type == 2/*&& sd->bl.id != dstsd->bl.id*/)
		return;
	
	if (
		((level = pc_isGM(sd)) > pc_isGM(dstsd) && level >= get_atcommand_level(AtCommand_Mute))
		|| (type == 2 && !level)) {
		clif_GM_silence(sd, dstsd, ((type == 2) ? 1 : type));
		if (battle_config.manner_system)
		{
			dstsd->status.manner -= limit;
			if(dstsd->status.manner < 0)
				sc_start(bl,SC_NOCHAT,100,0,0);
			else
			{
				dstsd->status.manner = 0;
				status_change_end(bl,SC_NOCHAT,-1);
			}
		}
		ShowDebug("GMReqNoChat: name:%s type:%d limit:%d manner:%d\n", dstsd->status.name, type, limit, dstsd->status.manner);
	}

	return;
}
/*==========================================
 * GMによるチャット禁止時間参照（？）
 *------------------------------------------
 */
void clif_parse_GMReqNoChatCount(int fd, struct map_session_data *sd)
{
	int tid;
	RFIFOHEAD(fd);
	tid = RFIFOL(fd,2);

	WFIFOHEAD(fd,packet_len_table[0x1e0]);
	WFIFOW(fd,0) = 0x1e0;
	WFIFOL(fd,2) = tid;
	sprintf((char*)WFIFOP(fd,6),"%d",tid);
//	memcpy(WFIFOP(fd,6), "TESTNAME", 24);
	WFIFOSET(fd, packet_len_table[0x1e0]);

	return;
}

void clif_parse_PMIgnore(int fd, struct map_session_data *sd) {	// Rewritten by [Yor]
	char output[512];
	char *nick; // S 00cf <nick>.24B <type>.B: 00 (/ex nick) deny speech from nick, 01 (/in nick) allow speech from nick
	int i, pos;
	RFIFOHEAD(fd);

	memset(output, '\0', sizeof(output));

	nick = (char*)RFIFOP(fd,2); // speed up
	RFIFOB(fd,NAME_LENGTH+1) = '\0'; // to be sure that the player name have at maximum 23 characters (nick range: [2]->[26])
	//printf("Ignore: char '%s' state: %d\n", nick, RFIFOB(fd,26));

	WFIFOHEAD(fd,packet_len_table[0xd1]);
	WFIFOW(fd,0) = 0x0d1; // R 00d1 <type>.B <fail>.B: type: 0: deny, 1: allow, fail: 0: success, 1: fail
	WFIFOB(fd,2) = RFIFOB(fd,26);
	// do nothing only if nick can not exist
	if (strlen(nick) < 4) {
		WFIFOB(fd,3) = 1; // fail
		WFIFOSET(fd, packet_len_table[0x0d1]);
		if (RFIFOB(fd,26) == 0) // type
			clif_wis_message(fd, wisp_server_name, "It's impossible to block this player.", strlen("It's impossible to block this player.") + 1);
		else
			clif_wis_message(fd, wisp_server_name, "It's impossible to unblock this player.", strlen("It's impossible to unblock this player.") + 1);
		return;
	// name can exist
	} else {
		// deny action (we add nick only if it's not already exist
		if (RFIFOB(fd,26) == 0) { // type
			pos = -1;
			for(i = 0; i < MAX_IGNORE_LIST; i++) {
				if (strcmp(sd->ignore[i].name, nick) == 0) {
					WFIFOB(fd,3) = 1; // fail
					WFIFOSET(fd, packet_len_table[0x0d1]);
					clif_wis_message(fd, wisp_server_name, "This player is already blocked.", strlen("This player is already blocked.") + 1);
					if (strcmp(wisp_server_name, nick) == 0) { // to found possible bot users who automaticaly ignore people.
						sprintf(output, "Character '%s' (account: %d) has tried AGAIN to block wisps from '%s' (wisp name of the server). Bot user?", sd->status.name, sd->status.account_id, wisp_server_name);
						intif_wis_message_to_gm(wisp_server_name, battle_config.hack_info_GM_level, output);
					}
					return;
				} else if (pos == -1 && sd->ignore[i].name[0] == '\0')
					pos = i;
			}
			// if a position is found and name not found, we add it in the list
			if (pos != -1) {
				memcpy(sd->ignore[pos].name, nick, NAME_LENGTH-1);
				WFIFOB(fd,3) = 0; // success
				WFIFOSET(fd, packet_len_table[0x0d1]);
				if (strcmp(wisp_server_name, nick) == 0) { // to found possible bot users who automaticaly ignore people.
					sprintf(output, "Character '%s' (account: %d) has tried to block wisps from '%s' (wisp name of the server). Bot user?", sd->status.name, sd->status.account_id, wisp_server_name);
					intif_wis_message_to_gm(wisp_server_name, battle_config.hack_info_GM_level, output);
					// send something to be inform and force bot to ignore twice... If GM receiving block + block again, it's a bot :)
					clif_wis_message(fd, wisp_server_name, "Add me in your ignore list, doesn't block my wisps.", strlen("Add me in your ignore list, doesn't block my wisps.") + 1);
				}
			} else {
				WFIFOB(fd,3) = 1; // fail
				WFIFOSET(fd, packet_len_table[0x0d1]);
				clif_wis_message(fd, wisp_server_name, "You can not block more people.", strlen("You can not block more people.") + 1);
				if (strcmp(wisp_server_name, nick) == 0) { // to found possible bot users who automaticaly ignore people.
					sprintf(output, "Character '%s' (account: %d) has tried to block wisps from '%s' (wisp name of the server). Bot user?", sd->status.name, sd->status.account_id, wisp_server_name);
					intif_wis_message_to_gm(wisp_server_name, battle_config.hack_info_GM_level, output);
				}
			}
		// allow action (we remove all same nicks if they exist)
		} else {
			pos = -1;
			for(i = 0; i < MAX_IGNORE_LIST; i++)
				if (strcmp(sd->ignore[i].name, nick) == 0) {
					memset(sd->ignore[i].name, 0, sizeof(sd->ignore[i].name));
					if (pos == -1) {
						WFIFOB(fd,3) = 0; // success
						WFIFOSET(fd, packet_len_table[0x0d1]);
						pos = i; // don't break, to remove ALL same nick
					}
				}
			if (pos == -1) {
				WFIFOB(fd,3) = 1; // fail
				WFIFOSET(fd, packet_len_table[0x0d1]);
				clif_wis_message(fd, wisp_server_name, "This player is not blocked by you.", strlen("This player is not blocked by you.") + 1);
			}
		}
	}

//	for(i = 0; i < MAX_IGNORE_LIST; i++) // for debug only
//		if (sd->ignore[i].name[0] != '\0')
//			printf("Ignored player: '%s'\n", sd->ignore[i].name);

	return;
}

void clif_parse_PMIgnoreAll(int fd, struct map_session_data *sd) { // Rewritten by [Yor]
	//printf("Ignore all: state: %d\n", RFIFOB(fd,2));
	RFIFOHEAD(fd);
	if (RFIFOB(fd,2) == 0) {// S 00d0 <type>len.B: 00 (/exall) deny all speech, 01 (/inall) allow all speech
                WFIFOHEAD(fd,packet_len_table[0xd2]);
		WFIFOW(fd,0) = 0x0d2; // R 00d2 <type>.B <fail>.B: type: 0: deny, 1: allow, fail: 0: success, 1: fail
		WFIFOB(fd,2) = 0;
		if (sd->ignoreAll == 0) {
			sd->ignoreAll = 1;
			WFIFOB(fd,3) = 0; // success
			WFIFOSET(fd, packet_len_table[0x0d2]);
		} else {
			WFIFOB(fd,3) = 1; // fail
			WFIFOSET(fd, packet_len_table[0x0d2]);
			clif_wis_message(fd, wisp_server_name, "You already block everyone.", strlen("You already block everyone.") + 1);
		}
	} else {
                WFIFOHEAD(fd,packet_len_table[0xd2]);
		WFIFOW(fd,0) = 0x0d2; // R 00d2 <type>.B <fail>.B: type: 0: deny, 1: allow, fail: 0: success, 1: fail
		WFIFOB(fd,2) = 1;
		if (sd->ignoreAll == 1) {
			sd->ignoreAll = 0;
			WFIFOB(fd,3) = 0; // success
			WFIFOSET(fd, packet_len_table[0x0d2]);
		} else {
			WFIFOB(fd,3) = 1; // fail
			WFIFOSET(fd, packet_len_table[0x0d2]);
			clif_wis_message(fd, wisp_server_name, "You already allow everyone.", strlen("You already allow everyone.") + 1);
		}
	}

	return;
}

/*==========================================
 * Wis拒否リスト
 *------------------------------------------
 */
 int pstrcmp(const void *a, const void *b)
{
	return strcmp((char *)a, (char *)b);
}
void clif_parse_PMIgnoreList(int fd,struct map_session_data *sd)
{
	int i,j=0,count=0;

	qsort (sd->ignore[0].name, MAX_IGNORE_LIST, sizeof(sd->ignore[0].name), pstrcmp);
	for(i = 0; i < MAX_IGNORE_LIST; i++){	//中身があるのを数える
		if(sd->ignore[i].name[0] != 0)
			count++;
	}
	WFIFOHEAD(fd, 4 + (NAME_LENGTH * count));
	WFIFOW(fd,0) = 0xd4;
	WFIFOW(fd,2) = 4 + (NAME_LENGTH * count);
	for(i = 0; i < MAX_IGNORE_LIST; i++){
		if(sd->ignore[i].name[0] != 0){
			memcpy(WFIFOP(fd, 4 + j * 24),sd->ignore[i].name, NAME_LENGTH);
			j++;
		}
	}
	WFIFOSET(fd, WFIFOW(fd,2));
	if(count >= MAX_IGNORE_LIST)	//満タンなら最後の1個を消す
		sd->ignore[MAX_IGNORE_LIST - 1].name[0] = 0;

	return;
}

/*==========================================
 * スパノビの/doridoriによるSPR2倍
 *------------------------------------------
 */
void clif_parse_NoviceDoriDori(int fd, struct map_session_data *sd) {
	int level;
	
	if ((sd->class_&MAPID_UPPERMASK) == MAPID_SUPER_NOVICE)
		sd->doridori_counter++;
	
	if ((sd->class_&MAPID_BASEMASK) == MAPID_TAEKWON
		&& sd->state.rest && (level = pc_checkskill(sd,TK_SPTIME)))
		sc_start(&sd->bl,SkillStatusChangeTable[TK_SPTIME],100,level,skill_get_time(TK_SPTIME, level));
	return;
}
/*==========================================
 * スパノビの爆裂波動
 *------------------------------------------
 */
void clif_parse_NoviceExplosionSpirits(int fd, struct map_session_data *sd)
{
	if(sd){
		int nextbaseexp=pc_nextbaseexp(sd);
		if (battle_config.etc_log){
			if(nextbaseexp != 0)
				ShowInfo("SuperNovice explosionspirits!! %d %d %d %d\n",sd->bl.id,sd->status.class_,sd->status.base_exp,(int)((double)1000*sd->status.base_exp/nextbaseexp));
			else
				ShowInfo("SuperNovice explosionspirits!! %d %d %d 000\n",sd->bl.id,sd->status.class_,sd->status.base_exp);
		}
		if((sd->class_&MAPID_UPPERMASK) == MAPID_SUPER_NOVICE && sd->status.base_exp > 0 && nextbaseexp > 0 && (int)((double)1000*sd->status.base_exp/nextbaseexp)%100==0){
			clif_skill_nodamage(&sd->bl,&sd->bl,MO_EXPLOSIONSPIRITS,5,
				sc_start(&sd->bl,SkillStatusChangeTable[MO_EXPLOSIONSPIRITS],100,
					5,skill_get_time(MO_EXPLOSIONSPIRITS,5)));
		}
	}
	return;
}

// random notes:
// 0x214: monster/player info ?

/*==========================================
 * Friends List
 *------------------------------------------
 */
void clif_friendslist_toggle(struct map_session_data *sd,int account_id, int char_id, int online)
{	//Toggles a single friend online/offline [Skotlex]
	int i;

	//Seek friend.
	for (i = 0; i < MAX_FRIENDS && sd->status.friends[i].char_id &&
		(sd->status.friends[i].char_id != char_id || sd->status.friends[i].account_id != account_id); i++);

	if(i == MAX_FRIENDS || sd->status.friends[i].char_id == 0)
		return; //Not found

	WFIFOHEAD(sd->fd,packet_len_table[0x206]);
	WFIFOW(sd->fd, 0) = 0x206;
	WFIFOL(sd->fd, 2) = sd->status.friends[i].account_id;
	WFIFOL(sd->fd, 6) = sd->status.friends[i].char_id;
	WFIFOB(sd->fd,10) = !online; //Yeah, a 1 here means "logged off", go figure... 
	
	WFIFOSET(sd->fd, packet_len_table[0x206]);
}

//Subfunction called from clif_foreachclient to toggle friends on/off [Skotlex]
int clif_friendslist_toggle_sub(struct map_session_data *sd,va_list ap)
{
	int account_id, char_id, online;
	account_id = va_arg(ap, int);
	char_id = va_arg(ap, int);
	online = va_arg(ap, int);
	clif_friendslist_toggle(sd, account_id, char_id, online);
	return 0;
}

//For sending the whole friends list.
void clif_friendslist_send(struct map_session_data *sd) {
	int i = 0, n;
	
	// Send friends list
	WFIFOHEAD(sd->fd, MAX_FRIENDS * 32 + 4);
	WFIFOW(sd->fd, 0) = 0x201;
	for(i = 0; i < MAX_FRIENDS && sd->status.friends[i].char_id; i++)
	{
		WFIFOL(sd->fd, 4 + 32 * i + 0) = sd->status.friends[i].account_id;
		WFIFOL(sd->fd, 4 + 32 * i + 4) = sd->status.friends[i].char_id;
		memcpy(WFIFOP(sd->fd, 4 + 32 * i + 8), &sd->status.friends[i].name, NAME_LENGTH);
	}

	if (i) {
		WFIFOW(sd->fd,2) = 4 + 32 * i;
		WFIFOSET(sd->fd, WFIFOW(sd->fd,2));
	}
	
	for (n = 0; n < i; n++)
	{	//Sending the online players
		if (map_charid2sd(sd->status.friends[n].char_id))
			clif_friendslist_toggle(sd, sd->status.friends[n].account_id, sd->status.friends[n].char_id, 1);
	}
}


// Status for adding friend - 0: successfull 1: not exist/rejected 2: over limit
void clif_friendslist_reqack(struct map_session_data *sd, struct map_session_data *f_sd, int type)
{
	int fd;
	nullpo_retv(sd);

	fd = sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x209]);
	WFIFOW(fd,0) = 0x209;
	WFIFOW(fd,2) = type;
	if (f_sd)
	{
		WFIFOW(fd,4) = f_sd->status.account_id;
		WFIFOW(fd,8) = f_sd->status.char_id;
		memcpy(WFIFOP(fd, 12), f_sd->status.name,NAME_LENGTH);
	}
	WFIFOSET(fd, packet_len_table[0x209]);
}

void clif_parse_FriendsListAdd(int fd, struct map_session_data *sd) {
	struct map_session_data *f_sd;
	int i, f_fd;
	RFIFOHEAD(fd);

	f_sd = map_nick2sd((char*)RFIFOP(fd,2));

	// Friend doesn't exist (no player with this name)
	if (f_sd == NULL) {
		clif_displaymessage(fd, msg_txt(3));
		return;
	}

	// @noask [LuzZza]
	if(f_sd->state.noask) {
		// Your request has been rejected by autoreject option.
		clif_displaymessage(fd, msg_txt(392));
		return;
	}

	// Friend already exists
	for (i = 0; i < MAX_FRIENDS && sd->status.friends[i].char_id != 0; i++) {
		if (sd->status.friends[i].char_id == f_sd->status.char_id) {
			clif_displaymessage(fd, "Friend already exists.");
			return;
		}
	}

	if (i == MAX_FRIENDS) {
		//No space, list full.
		clif_friendslist_reqack(sd, f_sd, 2);
		return;
	}
		
	f_fd = f_sd->fd;
	WFIFOHEAD(f_fd,packet_len_table[0x207]);
	WFIFOW(f_fd,0) = 0x207;
	WFIFOL(f_fd,2) = sd->status.account_id;
	WFIFOL(f_fd,6) = sd->status.char_id;
	memcpy(WFIFOP(f_fd,10), sd->status.name, NAME_LENGTH);
	WFIFOSET(f_fd, packet_len_table[0x207]);

	return;
}

void clif_parse_FriendsListReply(int fd, struct map_session_data *sd) {
	//<W: id> <L: Player 1 chara ID> <L: Player 1 AID> <B: Response>
	struct map_session_data *f_sd;
	int char_id, account_id;
	char reply;
	RFIFOHEAD(fd);

	account_id = RFIFOL(fd,2);
	char_id = RFIFOL(fd,6);
	reply = RFIFOB(fd,10);
	//printf ("reply: %d %d %d\n", char_id, id, reply);

	f_sd = map_id2sd(account_id); //The account id is the same as the bl.id of players.
	if (f_sd == NULL)
		return;
		
	if (reply == 0)
		clif_friendslist_reqack(f_sd, sd, 1);
	else {
		int i;
		// Find an empty slot
		for (i = 0; i < MAX_FRIENDS; i++)
			if (f_sd->status.friends[i].char_id == 0)
				break;
		if (i == MAX_FRIENDS) {
			clif_friendslist_reqack(f_sd, sd, 2);
			return;
		}

		f_sd->status.friends[i].account_id = sd->status.account_id;
		f_sd->status.friends[i].char_id = sd->status.char_id;
		memcpy(f_sd->status.friends[i].name, sd->status.name, NAME_LENGTH);
		clif_friendslist_reqack(f_sd, sd, 0);

//		clif_friendslist_send(sd); //This is not needed anymore.
	}

	return;
}

void clif_parse_FriendsListRemove(int fd, struct map_session_data *sd) {
	// 0x203 </o> <ID to be removed W 4B>
	int account_id, char_id;
	int i, j;
	RFIFOHEAD(fd);

	account_id = RFIFOL(fd,2);
	char_id = RFIFOL(fd,6);

	// Search friend
	for (i = 0; i < MAX_FRIENDS &&
		(sd->status.friends[i].char_id != char_id || sd->status.friends[i].account_id != account_id); i++);

	if (i == MAX_FRIENDS) {
		clif_displaymessage(fd, "Name not found in list.");
		return;
	}
		
	// move all chars down
	for(j = i + 1; j < MAX_FRIENDS; j++)
		memcpy(&sd->status.friends[j-1], &sd->status.friends[j], sizeof(sd->status.friends[0]));

	memset(&sd->status.friends[MAX_FRIENDS-1], 0, sizeof(sd->status.friends[MAX_FRIENDS-1]));
	clif_displaymessage(fd, "Friend removed");
	
	WFIFOHEAD(fd,packet_len_table[0x20a]);
	WFIFOW(fd,0) = 0x20a;
	WFIFOL(fd,2) = account_id;
	WFIFOL(fd,6) = char_id;
	WFIFOSET(fd, packet_len_table[0x20a]);
//	clif_friendslist_send(sd); //This is not needed anymore.

	return;
}

/*==========================================
 * /killall
 *------------------------------------------
 */
void clif_parse_GMKillAll(int fd,struct map_session_data *sd)
{
	char message[50];

	strncpy(message,sd->status.name, NAME_LENGTH);
	is_atcommand(fd, sd, strcat(message," : @kickall"),0);

	return;
}

/*==========================================
 * /pvpinfo
 *------------------------------------------
 */
void clif_parse_PVPInfo(int fd,struct map_session_data *sd)
{
	WFIFOHEAD(fd,packet_len_table[0x210]);
	WFIFOW(fd,0) = 0x210;
	//WFIFOL(fd,2) = 0;	// not sure what for yet
	//WFIFOL(fd,6) = 0;
	WFIFOL(fd,10) = sd->pvp_won;	// times won
	WFIFOL(fd,14) = sd->pvp_lost;	// times lost
	WFIFOL(fd,18) = sd->pvp_point;
	WFIFOSET(fd, packet_len_table[0x210]);

	return;
}

/*==========================================
 * /blacksmith
 *------------------------------------------
 */
void clif_parse_Blacksmith(int fd,struct map_session_data *sd)
{
	int i;
	char *name;

	WFIFOHEAD(fd,packet_len_table[0x219]);
	WFIFOW(fd,0) = 0x219;
	//Packet size limits this list to 10 elements. [Skotlex]
	for (i = 0; i < 10 && i < MAX_FAME_LIST; i++) {
		if (smith_fame_list[i].id > 0) {
			if (strcmp(smith_fame_list[i].name, "-") == 0 &&
				(name = map_charid2nick(smith_fame_list[i].id)) != NULL)
			{
				strncpy((char *)(WFIFOP(fd, 2 + 24 * i)), name, NAME_LENGTH);
			} else
				strncpy((char *)(WFIFOP(fd, 2 + 24 * i)), smith_fame_list[i].name, NAME_LENGTH);
		} else
			strncpy((char *)(WFIFOP(fd, 2 + 24 * i)), "None", 5);
		WFIFOL(fd, 242 + i * 4) = smith_fame_list[i].fame;
	}
	for(;i < 10; i++) { //In case the MAX is less than 10.
		strncpy((char *)(WFIFOP(fd, 2 + 24 * i)), "Unavailable", 12);
		WFIFOL(fd, 242 + i * 4) = 0;
	}

	WFIFOSET(fd, packet_len_table[0x219]);
}

int clif_fame_blacksmith(struct map_session_data *sd, int points)
{
	int fd = sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x21b]);
	WFIFOW(fd,0) = 0x21b;
	WFIFOL(fd,2) = points;
	WFIFOL(fd,6) = sd->status.fame;
	WFIFOSET(fd, packet_len_table[0x21b]);

	return 0;
}

/*==========================================
 * /alchemist
 *------------------------------------------
 */
void clif_parse_Alchemist(int fd,struct map_session_data *sd)
{
	int i;
	char *name;

	WFIFOHEAD(fd,packet_len_table[0x21a]);
	WFIFOW(fd,0) = 0x21a;
	//Packet size limits this list to 10 elements. [Skotlex]
	for (i = 0; i < 10 && i < MAX_FAME_LIST; i++) {
		if (chemist_fame_list[i].id > 0) {
			if (strcmp(chemist_fame_list[i].name, "-") == 0 &&
				(name = map_charid2nick(chemist_fame_list[i].id)) != NULL)
			{
				memcpy(WFIFOP(fd, 2 + 24 * i), name, NAME_LENGTH);
			} else
				memcpy(WFIFOP(fd, 2 + 24 * i), chemist_fame_list[i].name, NAME_LENGTH);
		} else
			memcpy(WFIFOP(fd, 2 + 24 * i), "None", NAME_LENGTH);
		WFIFOL(fd, 242 + i * 4) = chemist_fame_list[i].fame;
	}
	for(;i < 10; i++) { //In case the MAX is less than 10.
		memcpy(WFIFOP(fd, 2 + 24 * i), "Unavailable", NAME_LENGTH);
		WFIFOL(fd, 242 + i * 4) = 0;
	}

	WFIFOSET(fd, packet_len_table[0x21a]);
}

int clif_fame_alchemist(struct map_session_data *sd, int points)
{
	int fd = sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x21c]);
	WFIFOW(fd,0) = 0x21c;
	WFIFOL(fd,2) = points;
	WFIFOL(fd,6) = sd->status.fame;
	WFIFOSET(fd, packet_len_table[0x21c]);
	
	return 0;
}

/*==========================================
 * /taekwon
 *------------------------------------------
 */
void clif_parse_Taekwon(int fd,struct map_session_data *sd)
{
	int i;
	char *name;

	WFIFOHEAD(fd,packet_len_table[0x226]);
	WFIFOW(fd,0) = 0x226;
	//Packet size limits this list to 10 elements. [Skotlex]
	for (i = 0; i < 10 && i < MAX_FAME_LIST; i++) {
		if (taekwon_fame_list[i].id > 0) {
			if (strcmp(taekwon_fame_list[i].name, "-") == 0 &&
				(name = map_charid2nick(taekwon_fame_list[i].id)) != NULL)
			{
				memcpy(WFIFOP(fd, 2 + 24 * i), name, NAME_LENGTH);
			} else
				memcpy(WFIFOP(fd, 2 + 24 * i), taekwon_fame_list[i].name, NAME_LENGTH);
		} else
			memcpy(WFIFOP(fd, 2 + 24 * i), "None", NAME_LENGTH);
		WFIFOL(fd, 242 + i * 4) = taekwon_fame_list[i].fame;
	}
	for(;i < 10; i++) { //In case the MAX is less than 10.
		memcpy(WFIFOP(fd, 2 + 24 * i), "Unavailable", NAME_LENGTH);
		WFIFOL(fd, 242 + i * 4) = 0;
	}
	WFIFOSET(fd, packet_len_table[0x226]);
}

int clif_fame_taekwon(struct map_session_data *sd, int points)
{
	int fd = sd->fd;
	WFIFOHEAD(fd,packet_len_table[0x224]);
	WFIFOW(fd,0) = 0x224;
	WFIFOL(fd,2) = points;
	WFIFOL(fd,6) = sd->status.fame;
	WFIFOSET(fd, packet_len_table[0x224]);
	
	return 0;
}

/*==========================================
 * PK Ranking table?
 *------------------------------------------
 */
void clif_parse_RankingPk(int fd,struct map_session_data *sd)
{
	int i;

	WFIFOHEAD(fd,packet_len_table[0x238]);
	WFIFOW(fd,0) = 0x238;
	for(i=0;i<10;i++){
		memcpy(WFIFOP(fd,i*24+2), "Unknown", NAME_LENGTH);
		WFIFOL(fd,i*4+242) = 0;
	}
	WFIFOSET(fd, packet_len_table[0x238]);
	return;
}

/*==========================================
 * SG Feel save OK [Komurka]
 *------------------------------------------
 */
void clif_parse_FeelSaveOk(int fd,struct map_session_data *sd)
{
	char feel_var[3][NAME_LENGTH] = {"PC_FEEL_SUN","PC_FEEL_MOON","PC_FEEL_STAR"};
	int i;
	if (sd->menuskill_id != SG_FEEL)
		return;
	i = sd->menuskill_lv-1;
	if (i<0 || i > 2) return; //Bug?
	sd->feel_map[i].index = map[sd->bl.m].index;
	sd->feel_map[i].m = sd->bl.m;
	pc_setglobalreg(sd,feel_var[i],map[sd->bl.m].index);
	
	WFIFOHEAD(fd,packet_len_table[0x20e]);
	WFIFOW(fd,0)=0x20e;
	memcpy(WFIFOP(fd,2),map[sd->bl.m].name, MAP_NAME_LENGTH);
	WFIFOL(fd,26)=sd->bl.id;
	WFIFOW(fd,30)=i;
	WFIFOSET(fd, packet_len_table[0x20e]);
	
	clif_skill_nodamage(&sd->bl,&sd->bl,sd->menuskill_id,sd->menuskill_lv,1);
	sd->menuskill_lv = sd->menuskill_id = 0;
}

/*==========================================
 * Question about Star Glaldiator save map [Komurka]
 *------------------------------------------
 */
void clif_parse_ReqFeel(int fd, struct map_session_data *sd, int skilllv) {
	WFIFOHEAD(fd,packet_len_table[0x253]);
	WFIFOW(fd,0)=0x253;
	WFIFOSET(fd, packet_len_table[0x253]);
	sd->menuskill_id=SG_FEEL;
	sd->menuskill_lv=skilllv;
}

void clif_parse_AdoptRequest(int fd,struct map_session_data *sd) {
	//TODO: add somewhere the adopt code, checks for exploits, etc, etc.
	//Missing packets are the client's reply packets to the adopt request one. 
	//[Skotlex]
	int account_id;
	struct map_session_data *sd2;
	RFIFOHEAD(fd);
	
	account_id = RFIFOL(fd,2);
	sd2 = map_id2sd(account_id);
	if(sd2 && sd2->fd && sd2 != sd && sd2->status.party_id == sd->status.party_id) {	//FIXME: No checks whatsoever are in place yet!
		fd = sd2->fd;
		WFIFOHEAD(fd,packet_len_table[0x1f9]);
		WFIFOW(fd,0)=0x1f9;
		WFIFOSET(fd, packet_len_table[0x1f9]);
	}
}
/*==========================================
 * パケットデバッグ
 *------------------------------------------
 */
void clif_parse_debug(int fd,struct map_session_data *sd)
{
	int i, cmd, len;
	RFIFOHEAD(fd);

	cmd = RFIFOW(fd,0);
	len = sd?packet_db[sd->packet_ver][cmd].len:RFIFOREST(fd); //With no session, just read the remaining in the buffer.
	ShowDebug("packet debug 0x%4X\n",cmd);
	printf("---- 00-01-02-03-04-05-06-07-08-09-0A-0B-0C-0D-0E-0F");
	for(i=0;i<len;i++){
		if((i&15)==0)
			printf("\n%04X ",i);
		printf("%02X ",RFIFOB(fd,i));
	}
	printf("\n");
}

/*==========================================
 * クライアントからのパケット解析
 * socket.cのdo_parsepacketから呼び出される
 *------------------------------------------
 */
int clif_parse(int fd) {
	int packet_len = 0, cmd, packet_ver, dump = 0;
	struct map_session_data *sd;
	RFIFOHEAD(fd);

	if (fd <= 0)
	{	//Just in case, there are some checks for this later down below anyway which should be removed. [Skotlex]
		ShowError("clif_parse: Received invalid session %d\n", fd);
		return 0;
	}

	sd = (struct map_session_data*)session[fd]->session_data;

	// 接続が切れてるので後始末
	if (!chrif_isconnect() && kick_on_disconnect)
	{
		ShowInfo("Closing session #%d (Not connected to Char server)\n", fd);
		if (sd && sd->state.auth)
			clif_quitsave(fd, sd); // the function doesn't send to inter-server/char-server if it is not connected [Yor]
		do_close(fd);
		return 0;
	} else
	if (session[fd]->eof) {
		if (sd && sd->state.autotrade) {
			//Disassociate character from the socket connection.
			session[fd]->session_data = NULL;
			sd->fd = 0;
			ShowInfo("%sCharacter '"CL_WHITE"%s"CL_RESET"' logged off (using @autotrade).\n", (pc_isGM(sd))?"GM ":"",sd->status.name); // Player logout display [Valaris]
		} else if (sd && sd->state.auth) {
			clif_quitsave(fd, sd); // the function doesn't send to inter-server/char-server if it is not connected [Yor]
			if (sd->status.name != NULL)
				ShowInfo("%sCharacter '"CL_WHITE"%s"CL_RESET"' logged off.\n", (pc_isGM(sd))?"GM ":"",sd->status.name); // Player logout display [Valaris]
			else
				ShowInfo("%sCharacter with Account ID '"CL_WHITE"%d"CL_RESET"' logged off.\n", (pc_isGM(sd))?"GM ":"", sd->bl.id); // Player logout display [Yor]
		} else {
			unsigned char *ip = (unsigned char *) &session[fd]->client_addr.sin_addr;
			ShowInfo("Player not identified with IP '"CL_WHITE"%d.%d.%d.%d"CL_RESET"' logged off.\n", ip[0],ip[1],ip[2],ip[3]);
		}
		do_close(fd);
		return 0;
	}

	if (RFIFOREST(fd) < 2)
		return 0;

//	printf("clif_parse: connection #%d, packet: 0x%x (with being read: %d bytes).\n", fd, RFIFOW(fd,0), RFIFOREST(fd));

	cmd = RFIFOW(fd,0);

	// 管理用パケット処理
	if (cmd >= 30000) {
		switch(cmd) {
		case 0x7530: { //Why are we letting people know which version we are running?
			WFIFOHEAD(fd, 10);
			WFIFOW(fd,0) = 0x7531;
			WFIFOB(fd,2) = ATHENA_MAJOR_VERSION;
			WFIFOB(fd,3) = ATHENA_MINOR_VERSION;
			WFIFOB(fd,4) = ATHENA_REVISION;
			WFIFOB(fd,5) = ATHENA_RELEASE_FLAG;
			WFIFOB(fd,6) = ATHENA_OFFICIAL_FLAG;
			WFIFOB(fd,7) = ATHENA_SERVER_MAP;
			WFIFOW(fd,8) = ATHENA_MOD_VERSION;
			WFIFOSET(fd,10);
			RFIFOSKIP(fd,2);
			break;
		}
		case 0x7532: // 接続の切断
			ShowWarning("clif_parse: session #%d disconnected for sending packet 0x04%x\n", fd, cmd);
			session[fd]->eof=1;
			break;
		default:
			ShowWarning("Unknown incoming packet (command: 0x%04x, session: %d), disconnecting.\n", cmd, fd);
			session[fd]->eof=1;
			break;
		}
		return 0;
	}

	// get packet version before to parse
	packet_ver = 0;
	if (sd) {
		packet_ver = sd->packet_ver;
		if (packet_ver < 0 || packet_ver > MAX_PACKET_VER) {	// This should never happen unless we have some corrupted memory issues :X [Skotlex]
			session[fd]->eof = 1;
			return 0;
		}
	} else {
		// check authentification packet to know packet version
		packet_ver = clif_guess_PacketVer(fd, 0);
		// check if version is accepted
		if (packet_ver < 5 ||	// reject really old client versions
			(packet_ver <= 9 && (battle_config.packet_ver_flag & 1) == 0) ||	// older than 6sept04
			(packet_ver > 9 && (battle_config.packet_ver_flag & 1<<(packet_ver-9)) == 0) ||
			packet_ver > MAX_PACKET_VER)	// no packet version support yet
		{
			ShowInfo("clif_parse: Disconnecting session #%d for not having latest client version (has version %d).\n", fd, packet_ver);
			WFIFOHEAD(fd, 23);
			WFIFOW(fd,0) = 0x6a;
			WFIFOB(fd,2) = 5; // 05 = Game's EXE is not the latest version
			WFIFOSET(fd,23);
			packet_len = RFIFOREST(fd);
			RFIFOSKIP(fd, packet_len);
			clif_setwaitclose(fd);
			if (session[fd]->func_send)  //socket.c doesn't wants to send the data when left on it's own... [Skotlex]
				session[fd]->func_send(fd);
			return 0;
		}
	}

	// ゲーム用以外パケットか、認証を終える前に0072以外が来たら、切断する
	if (cmd >= MAX_PACKET_DB || packet_db[packet_ver][cmd].len == 0) {	// if packet is not inside these values: session is incorrect?? or auth packet is unknown
		ShowWarning("clif_parse: Received unsupported packet (packet 0x%04x, %d bytes received), disconnecting session #%d.\n", cmd, RFIFOREST(fd), fd);
		session[fd]->eof = 1;
		return 0;
	}

	// パケット長を計算
	packet_len = packet_db[packet_ver][cmd].len;
	if (packet_len == -1) {
		if (RFIFOREST(fd) < 4)
			return 0; // 可変長パケットで長さの所までデータが来てない

		packet_len = RFIFOW(fd,2);
		if (packet_len < 4 || packet_len > 32768) {
			ShowWarning("clif_parse: Packet 0x%04x specifies invalid packet_len (%d), disconnecting session #%d.\n", cmd, packet_len, fd);
			session[fd]->eof =1;
			return 0;
		}
	}
	if ((int)RFIFOREST(fd) < packet_len)
		return 0; // まだ1パケット分データが揃ってない

	#if DUMP_ALL_PACKETS
		dump = 1;
	#endif

	if (sd && sd->state.auth == 1 && sd->state.waitingdisconnect == 1) { // 切断待ちの場合パケットを処理しない

	} else if (packet_db[packet_ver][cmd].func) {
		if (sd
			|| packet_db[packet_ver][cmd].func == clif_parse_WantToConnection
			|| packet_db[packet_ver][cmd].func == clif_parse_debug
		)	//Only execute the function when there's an sd (except for debug/wanttoconnect packets)
			packet_db[packet_ver][cmd].func(fd, sd);
	} else {
		// 不明なパケット
		if (battle_config.error_log) {
#if DUMP_UNKNOWN_PACKET
			{
				int i;
				FILE *fp;
				char packet_txt[256] = "save/packet.txt";
				time_t now;
				dump = 1;

				if ((fp = fopen(packet_txt, "a")) == NULL) {
					ShowError("clif.c: cant write [%s] !!! data is lost !!!\n", packet_txt);
					return 1;
				} else {
					time(&now);
					if (sd && sd->state.auth) {
						if (sd->status.name != NULL)
							fprintf(fp, "%sPlayer with account ID %d (character ID %d, player name %s) sent wrong packet:\n",
							        asctime(localtime(&now)), sd->status.account_id, sd->status.char_id, sd->status.name);
						else
							fprintf(fp, "%sPlayer with account ID %d sent wrong packet:\n", asctime(localtime(&now)), sd->bl.id);
					} else if (sd) // not authentified! (refused by char-server or disconnect before to be authentified)
						fprintf(fp, "%sPlayer with account ID %d sent wrong packet:\n", asctime(localtime(&now)), sd->bl.id);

					fprintf(fp, "\t---- 00-01-02-03-04-05-06-07-08-09-0A-0B-0C-0D-0E-0F");
					for(i = 0; i < packet_len; i++) {
						if ((i & 15) == 0)
							fprintf(fp, "\n\t%04X ", i);
						fprintf(fp, "%02X ", RFIFOB(fd,i));
					}
					fprintf(fp, "\n\n");
					fclose(fp);
				}
			}
#endif
		}
	}

	if (dump) {
		int i;
		if (fd)
			ShowDebug("\nclif_parse: session #%d, packet 0x%04x, length %d, version %d\n", fd, cmd, packet_len, packet_ver);
		printf("---- 00-01-02-03-04-05-06-07-08-09-0A-0B-0C-0D-0E-0F");
		for(i = 0; i < packet_len; i++) {
			if ((i & 15) == 0)
				printf("\n%04X ",i);
			printf("%02X ", RFIFOB(fd,i));
		}
		printf("\n");
		if (sd && sd->state.auth) {
			if (sd->status.name != NULL)
				printf("\nAccount ID %d, character ID %d, player name %s.\n",
			       sd->status.account_id, sd->status.char_id, sd->status.name);
			else
				printf("\nAccount ID %d.\n", sd->bl.id);
		} else if (sd) // not authentified! (refused by char-server or disconnect before to be authentified)
			printf("\nAccount ID %d.\n", sd->bl.id);
	}

	RFIFOSKIP(fd, packet_len);

	return 0;
}

/*==========================================
 * パケットデータベース読み込み
 *------------------------------------------
 */
static int packetdb_readdb(void)
{
	FILE *fp;
	char line[1024];
	int ln=0;
	int cmd,i,j,k,packet_ver;
	char *str[64],*p,*str2[64],*p2,w1[64],w2[64];

	struct {
		void (*func)(int, struct map_session_data *);
		char *name;
	} clif_parse_func[]={
		{clif_parse_WantToConnection,"wanttoconnection"},
		{clif_parse_LoadEndAck,"loadendack"},
		{clif_parse_TickSend,"ticksend"},
		{clif_parse_WalkToXY,"walktoxy"},
		{clif_parse_QuitGame,"quitgame"},
		{clif_parse_GetCharNameRequest,"getcharnamerequest"},
		{clif_parse_GlobalMessage,"globalmessage"},
		{clif_parse_MapMove,"mapmove"},
		{clif_parse_ChangeDir,"changedir"},
		{clif_parse_Emotion,"emotion"},
		{clif_parse_HowManyConnections,"howmanyconnections"},
		{clif_parse_ActionRequest,"actionrequest"},
		{clif_parse_Restart,"restart"},
		{clif_parse_Wis,"wis"},
		{clif_parse_GMmessage,"gmmessage"},
		{clif_parse_TakeItem,"takeitem"},
		{clif_parse_DropItem,"dropitem"},
		{clif_parse_UseItem,"useitem"},
		{clif_parse_EquipItem,"equipitem"},
		{clif_parse_UnequipItem,"unequipitem"},
		{clif_parse_NpcClicked,"npcclicked"},
		{clif_parse_NpcBuySellSelected,"npcbuysellselected"},
		{clif_parse_NpcBuyListSend,"npcbuylistsend"},
		{clif_parse_NpcSellListSend,"npcselllistsend"},
		{clif_parse_CreateChatRoom,"createchatroom"},
		{clif_parse_ChatAddMember,"chataddmember"},
		{clif_parse_ChatRoomStatusChange,"chatroomstatuschange"},
		{clif_parse_ChangeChatOwner,"changechatowner"},
		{clif_parse_KickFromChat,"kickfromchat"},
		{clif_parse_ChatLeave,"chatleave"},
		{clif_parse_TradeRequest,"traderequest"},
		{clif_parse_TradeAck,"tradeack"},
		{clif_parse_TradeAddItem,"tradeadditem"},
		{clif_parse_TradeOk,"tradeok"},
		{clif_parse_TradeCancel,"tradecancel"},
		{clif_parse_TradeCommit,"tradecommit"},
		{clif_parse_StopAttack,"stopattack"},
		{clif_parse_PutItemToCart,"putitemtocart"},
		{clif_parse_GetItemFromCart,"getitemfromcart"},
		{clif_parse_RemoveOption,"removeoption"},
		{clif_parse_ChangeCart,"changecart"},
		{clif_parse_StatusUp,"statusup"},
		{clif_parse_SkillUp,"skillup"},
		{clif_parse_UseSkillToId,"useskilltoid"},
		{clif_parse_UseSkillToPos,"useskilltopos"},
		{clif_parse_UseSkillToPosMoreInfo,"useskilltoposinfo"},
		{clif_parse_UseSkillMap,"useskillmap"},
		{clif_parse_RequestMemo,"requestmemo"},
		{clif_parse_ProduceMix,"producemix"},
		{clif_parse_NpcSelectMenu,"npcselectmenu"},
		{clif_parse_NpcNextClicked,"npcnextclicked"},
		{clif_parse_NpcAmountInput,"npcamountinput"},
		{clif_parse_NpcStringInput,"npcstringinput"},
		{clif_parse_NpcCloseClicked,"npccloseclicked"},
		{clif_parse_ItemIdentify,"itemidentify"},
		{clif_parse_SelectArrow,"selectarrow"},
		{clif_parse_AutoSpell,"autospell"},
		{clif_parse_UseCard,"usecard"},
		{clif_parse_InsertCard,"insertcard"},
		{clif_parse_RepairItem,"repairitem"},
		{clif_parse_WeaponRefine,"weaponrefine"},
		{clif_parse_SolveCharName,"solvecharname"},
		{clif_parse_ResetChar,"resetchar"},
		{clif_parse_LGMmessage,"lgmmessage"},
		{clif_parse_MoveToKafra,"movetokafra"},
		{clif_parse_MoveFromKafra,"movefromkafra"},
		{clif_parse_MoveToKafraFromCart,"movetokafrafromcart"},
		{clif_parse_MoveFromKafraToCart,"movefromkafratocart"},
		{clif_parse_CloseKafra,"closekafra"},
		{clif_parse_CreateParty,"createparty"},
		{clif_parse_CreateParty2,"createparty2"},
		{clif_parse_PartyInvite,"partyinvite"},
		{clif_parse_ReplyPartyInvite,"replypartyinvite"},
		{clif_parse_LeaveParty,"leaveparty"},
		{clif_parse_RemovePartyMember,"removepartymember"},
		{clif_parse_PartyChangeOption,"partychangeoption"},
		{clif_parse_PartyMessage,"partymessage"},
		{clif_parse_CloseVending,"closevending"},
		{clif_parse_VendingListReq,"vendinglistreq"},
		{clif_parse_PurchaseReq,"purchasereq"},
		{clif_parse_OpenVending,"openvending"},
		{clif_parse_CreateGuild,"createguild"},
		{clif_parse_GuildCheckMaster,"guildcheckmaster"},
		{clif_parse_GuildRequestInfo,"guildrequestinfo"},
		{clif_parse_GuildChangePositionInfo,"guildchangepositioninfo"},
		{clif_parse_GuildChangeMemberPosition,"guildchangememberposition"},
		{clif_parse_GuildRequestEmblem,"guildrequestemblem"},
		{clif_parse_GuildChangeEmblem,"guildchangeemblem"},
		{clif_parse_GuildChangeNotice,"guildchangenotice"},
		{clif_parse_GuildInvite,"guildinvite"},
		{clif_parse_GuildReplyInvite,"guildreplyinvite"},
		{clif_parse_GuildLeave,"guildleave"},
		{clif_parse_GuildExplusion,"guildexplusion"},
		{clif_parse_GuildMessage,"guildmessage"},
		{clif_parse_GuildRequestAlliance,"guildrequestalliance"},
		{clif_parse_GuildReplyAlliance,"guildreplyalliance"},
		{clif_parse_GuildDelAlliance,"guilddelalliance"},
		{clif_parse_GuildOpposition,"guildopposition"},
		{clif_parse_GuildBreak,"guildbreak"},
		{clif_parse_PetMenu,"petmenu"},
		{clif_parse_CatchPet,"catchpet"},
		{clif_parse_SelectEgg,"selectegg"},
		{clif_parse_SendEmotion,"sendemotion"},
		{clif_parse_ChangePetName,"changepetname"},
		{clif_parse_GMKick,"gmkick"},
		{clif_parse_GMHide,"gmhide"},
		{clif_parse_GMReqNoChat,"gmreqnochat"},
		{clif_parse_GMReqNoChatCount,"gmreqnochatcount"},
		{clif_parse_NoviceDoriDori,"sndoridori"},
		{clif_parse_NoviceExplosionSpirits,"snexplosionspirits"},
		{clif_parse_PMIgnore,"wisexin"},
		{clif_parse_PMIgnoreList,"wisexlist"},
		{clif_parse_PMIgnoreAll,"wisall"},
		{clif_parse_FriendsListAdd,"friendslistadd"},
		{clif_parse_FriendsListRemove,"friendslistremove"},
		{clif_parse_FriendsListReply,"friendslistreply"},
		{clif_parse_GMKillAll,"killall"},
		{clif_parse_Recall,"summon"},
		{clif_parse_GM_Monster_Item,"itemmonster"},
		{clif_parse_Shift,"shift"},
		{clif_parse_Blacksmith,"blacksmith"},
		{clif_parse_Alchemist,"alchemist"},
		{clif_parse_Taekwon,"taekwon"},
		{clif_parse_RankingPk,"rankingpk"},
		{clif_parse_FeelSaveOk,"feelsaveok"},
		{clif_parse_AdoptRequest,"adopt"},
		{clif_parse_debug,"debug"},
		//[blackhole89]
		{clif_parse_ChangeHomunculusName,"changehomunculusname"},
		{clif_parse_QueryHomunPos,"queryhomunpos"},
		{clif_parse_HMoveTo,"hmoveto"},
		{clif_parse_HAttack,"hattack"},
		{NULL,NULL}
	};

	sprintf(line, "%s/packet_db.txt", db_path);
	if( (fp=fopen(line,"r"))==NULL ){
		ShowFatalError("can't read %s\n", line);
		exit(1);
	}

	clif_config.packet_db_ver = MAX_PACKET_VER;
	packet_ver = MAX_PACKET_VER;	// read into packet_db's version by default
	while(fgets(line,1020,fp)){
		if(line[0]=='/' && line[1]=='/')
			continue;
		if (sscanf(line,"%[^:]: %[^\r\n]",w1,w2) == 2) {
			if(strcmpi(w1,"packet_ver")==0) {
				int prev_ver = packet_ver;
				packet_ver = atoi(w2);
				if (packet_ver > MAX_PACKET_VER)
				{	//Check to avoid overflowing. [Skotlex]
					ShowWarning("The packet_db table only has support up to version %d\n", MAX_PACKET_VER);
					break;
				}
				// copy from previous version into new version and continue
				// - indicating all following packets should be read into the newer version
				memcpy(&packet_db[packet_ver], &packet_db[prev_ver], sizeof(packet_db[0]));
				continue;
			} else if(strcmpi(w1,"packet_db_ver")==0) {
				//This is the preferred version.
				if(strcmpi(w2,"default")==0)
					clif_config.packet_db_ver = MAX_PACKET_VER;
				else {
					// to manually set the packet DB version
					clif_config.packet_db_ver = atoi(w2);
					// check for invalid version
					if (clif_config.packet_db_ver > MAX_PACKET_VER ||
						clif_config.packet_db_ver < 0)
						clif_config.packet_db_ver = MAX_PACKET_VER;
				}
				continue;
			}
		}

		memset(str,0,sizeof(str));
		for(j=0,p=line;j<4 && p;j++){
			str[j]=p;
			p=strchr(p,',');
			if(p) *p++=0;
		}
		if(str[0]==NULL)
			continue;
		cmd=strtol(str[0],(char **)NULL,0);
		if(cmd<=0 || cmd>=MAX_PACKET_DB)
			continue;
		if(str[1]==NULL){
			ShowError("packet_db: packet len error\n");
			continue;
		}
		k = atoi(str[1]);
		packet_db[packet_ver][cmd].len = k;

		if(str[2]==NULL){
			packet_db[packet_ver][cmd].func = NULL;
			ln++;
			continue;
		}
		for(j=0;j<sizeof(clif_parse_func)/sizeof(clif_parse_func[0]);j++){
			if(clif_parse_func[j].name != NULL &&
				strcmp(str[2],clif_parse_func[j].name)==0)
			{
				if (packet_db[packet_ver][cmd].func != clif_parse_func[j].func)
				{	//If we are updating a function, we need to zero up the previous one. [Skotlex]
					for(i=0;i<MAX_PACKET_DB;i++){
						if (packet_db[packet_ver][i].func == clif_parse_func[j].func)
						{	
							memset(&packet_db[packet_ver][i], 0, sizeof(struct packet_db));
							break;
						}
					}
				}
				packet_db[packet_ver][cmd].func = clif_parse_func[j].func;
				break;
			}
		}
		// set the identifying cmd for the packet_db version
		if (strcmp(str[2],"wanttoconnection")==0)
			clif_config.connect_cmd[packet_ver] = cmd;
			
		if(str[3]==NULL){
			ShowError("packet_db: packet error\n");
			exit(1);
		}
		for(j=0,p2=str[3];p2;j++){
			str2[j]=p2;
			p2=strchr(p2,':');
			if(p2) *p2++=0;
			k = atoi(str2[j]);
			// if (packet_db[packet_ver][cmd].pos[j] != k && clif_config.prefer_packet_db)	// not used for now
			packet_db[packet_ver][cmd].pos[j] = k;
		}

		ln++;
//		if(packet_db[clif_config.packet_db_ver][cmd].len > 2 /* && packet_db[cmd].pos[0] == 0 */)
//			printf("packet_db ver %d: %d 0x%x %d %s %p\n",packet_ver,ln,cmd,packet_db[packet_ver][cmd].len,str[2],packet_db[packet_ver][cmd].func);
	}
	fclose(fp);
	if (!clif_config.connect_cmd[clif_config.packet_db_ver])
	{	//Locate the nearest version that we still support. [Skotlex]
		for(j = clif_config.packet_db_ver; j >= 0 && !clif_config.connect_cmd[j]; j--);
		
		clif_config.packet_db_ver = j?j:MAX_PACKET_VER;
	}
	ShowStatus("Done reading packet database from '"CL_WHITE"%s"CL_RESET"'. Using default packet version: "CL_WHITE"%d"CL_RESET".\n", "packet_db.txt", clif_config.packet_db_ver);
	return 0;
}

/*==========================================
 *
 *------------------------------------------
 */
int do_init_clif(void) {
#ifndef __WIN32
	int i;
#endif
	
	clif_config.packet_db_ver = -1; // the main packet version of the DB
	memset(clif_config.connect_cmd, 0, sizeof(clif_config.connect_cmd)); //The default connect command will be determined after reading the packet_db [Skotlex]

	memset(packet_db,0,sizeof(packet_db));
	//Using the packet_db file is the only way to set up packets now [Skotlex]
	packetdb_readdb();

	set_defaultparse(clif_parse);
#ifdef __WIN32
	//if (!make_listen_port(map_port)) {
	if (!make_listen_bind(bind_ip,map_port)) {
		ShowFatalError("cant bind game port\n");
		exit(1);
	}
#else
	for(i = 0; i < 10; i++) {
		//if (make_listen_port(map_port))
		if (make_listen_bind(bind_ip,map_port))
			break;
		sleep(20);
	}
	if (i == 10) {
		ShowFatalError("cant bind game port\n");
		exit(1);
	}
#endif

	add_timer_func_list(clif_waitclose, "clif_waitclose");
	add_timer_func_list(clif_clearchar_delay_sub, "clif_clearchar_delay_sub");
	add_timer_func_list(clif_delayquit, "clif_delayquit");
	add_timer_func_list(clif_nighttimer, "clif_nighttimer");
	add_timer_func_list(clif_walktoxy_timer, "clif_walktoxy_timer");
	return 0;
}

