#ifndef INC_1OOM_GAME_MSG_H
#define INC_1OOM_GAME_MSG_H

#include "bits.h"

#define GAME_MSG_EN_U8(_m_, _v_)    (_m_)->buf[(_m_)->len++] = (_v_)
#define GAME_MSG_EN_U16(_m_, _v_)   SET_LE_16(&((_m_)->buf[(_m_)->len]), (_v_)), (_m_)->len += 2
#define GAME_MSG_EN_U32(_m_, _v_)   SET_LE_32(&((_m_)->buf[(_m_)->len]), (_v_)), (_m_)->len += 4

#define GAME_MSG_EN_TBL_U8(_m_, _v_, _n_)   do { uint8_t *p_ = &((_m_)->buf[(_m_)->len]); for (int i_ = 0; i_ < (_n_); ++i_) { p_[i_] = (_v_)[i_]; } (_m_)->len += (_n_); } while (0)

#define GAME_MSGO_PTR(_g_, _pi_)    (&((_g_)->gaux->msgo[_pi_]))
#define GAME_MSGI_PTR(_g_, _pi_)    (&((_g_)->gaux->msgi[_pi_]))
#define GAME_MSGO_DPTR(_g_, _pi_)   (&((_g_)->gaux->msgo[_pi_].buf[4]))
#define GAME_MSGI_DPTR(_g_, _pi_)   (&((_g_)->gaux->msgi[_pi_].buf[4]))

#define GAME_MSG_EN_HDR(_m_, _v_)   do { (_m_)->len = 0; GAME_MSG_EN_U16(_m_, _v_); GAME_MSG_EN_U16(_m_, 0); } while (0)
#define GAME_MSG_EN_LEN(_m_)        SET_LE_16(&((_m_)->buf[2]), (_m_)->len - 4)

#define GAME_MSGO_EN_U8(_g_, _pi_, _v_)     GAME_MSG_EN_U8(GAME_MSGO_PTR(_g_, _pi_), (_v_))
#define GAME_MSGO_EN_U16(_g_, _pi_, _v_)    GAME_MSG_EN_U16(GAME_MSGO_PTR(_g_, _pi_), (_v_))
#define GAME_MSGO_EN_U32(_g_, _pi_, _v_)    GAME_MSG_EN_U32(GAME_MSGO_PTR(_g_, _pi_), (_v_))

#define GAME_MSGO_EN_TBL_U8(_g_, _pi_, _v_, _n_)     GAME_MSG_EN_TBL_U8(GAME_MSGO_PTR(_g_, _pi_), (_v_), (_n_))

#define GAME_MSGO_EN_HDR(_g_, _pi_, _v_)    GAME_MSG_EN_HDR(GAME_MSGO_PTR(_g_, _pi_), (_v_))
#define GAME_MSGO_EN_LEN(_g_, _pi_)         GAME_MSG_EN_LEN(GAME_MSGO_PTR(_g_, _pi_))
#define GAME_MSGO_ADD_LEN(_g_, _pi_, _v_)   GAME_MSGO_PTR(_g_, _pi_)->len += (_v_)

#define GAME_MSGI_DE_ID(_g_, _pi_)      GET_LE_16(&(GAME_MSGI_PTR(_g_, _pi_)->buf[0]))
#define GAME_MSGO_DE_ID(_g_, _pi_)      GET_LE_16(&(GAME_MSGO_PTR(_g_, _pi_)->buf[0]))
#define GAME_MSGI_DE_LEN(_g_, _pi_)     GET_LE_16(&(GAME_MSGI_PTR(_g_, _pi_)->buf[2]))
#define GAME_MSGO_DE_LEN(_g_, _pi_)     GET_LE_16(&(GAME_MSGO_PTR(_g_, _pi_)->buf[2]))

#define GAME_MSGI_LEN(_g_, _pi_)      ((_g_)->gaux->msgi[_pi_].len)
#define GAME_MSGO_LEN(_g_, _pi_)      ((_g_)->gaux->msgo[_pi_].len)
#define GAME_MSGI_HANDLED(_g_, _pi_)  (_g_)->gaux->msgi[_pi_].len = 0

#define GAME_MSG_ID_NONE        0x0000
#define GAME_MSG_ID_CMD_STATUS  0x0002/*u16 status: 1=ok, 2..=error*/
#define GAME_MSG_ID_GAME_DATA   0x0008/*...*/
#define GAME_MSG_ID_TURN_INPUT  0x0011/*...*/
#define GAME_MSG_ID_SEND_FLEET  0x0012/*u8 from, u8 to, u16 ships[6]*/
#define GAME_MSG_ID_REDIR_FLEET 0x0013/*u8 to, u8 dest, u16 x, u16 y, u16 ships[6]*/
#define GAME_MSG_ID_REDIR_TRANS 0x0014/*u8 to, u8 dest, u16 x, u16 y, u16 pop*/
#define GAME_MSG_ID_SEND_BC     0x0015/*u8 to, u32 bc*/
#define GAME_MSG_ID_SCRAP_BASES 0x0016/*u8 planet, u16 num*/
#define GAME_MSG_ID_SCRAP_SHIP  0x0017/*u8 shipi, u8 for_new*/
#define GAME_MSG_ID_DESIGN_SHIP 0x0018/*...*/
#define GAME_MSG_ID_TURN_MOVE   0x0020/*...*/
#define GAME_MSG_ID_EXPLORE     0x0021/*...*/
#define GAME_MSG_ID_COLONIZEQ   0x0022/*...*/
#define GAME_MSG_ID_COLONIZEA   0x0023/*u8 planet, u8 colonize, char name[0xc]*/
#define GAME_MSG_ID_GROUND      0x0024/*...*/
#define GAME_MSG_ID_BOMBARD     0x0025/*N x { u8 planet|ff, u8 owner, u16 popdmg, u16 factdmg }*/
#define GAME_MSG_ID_BOMBQ       0x0026/*u8 planet, u16 pop, u16 fact, u16 inbound*/
#define GAME_MSG_ID_BOMBA       0x0027/*u8 planet, u8 bomb*/
#define GAME_MSG_ID_NEWTECH     0x0028/*...*/
#define GAME_MSG_ID_NEXTTECH    0x0029/*u8 field, u8 tech*/
#define GAME_MSG_ID_FRAMEESP    0x002a/*u8 newtech_i, u8 player*/
#define GAME_MSG_ID_STEALQ      0x002b/*u8 target, u8 fields_mask*/
#define GAME_MSG_ID_STEALA      0x002c/*u8 target, u8 field*/
#define GAME_MSG_ID_STOLEN      0x002d/*N x { u8 spy, u8 field, u8 tech }*/
#define GAME_MSG_ID_SABQ        0x002e/*u8 target*/
#define GAME_MSG_ID_SABA        0x002f/*u8 target, u8 act, u8 planet*/
#define GAME_MSG_ID_SABOTAGE    0x0030/*...*/
#define GAME_MSG_ID_FRAMESAB    0x0031/*u8 target, u8 scapegoat*/
#define GAME_MSG_ID_TRANSSHOT   0x0032/*N x { u8 planet|ff, u8 trowner }*/
#define GAME_MSG_ID_COUNCILS    0x0033/*u16 total_votes, u8 candidate[2], u8 N, N x { u8 player, u8 votes, u8 voted }*/
#define GAME_MSG_ID_VOTED       0x0034/*u8 voter_index, u8 voted*/
#define GAME_MSG_ID_VOTEQ       0x0035
#define GAME_MSG_ID_VOTEA       0x0036/*u8 voted: 0=abstain, 1,2=candidate*/
#define GAME_MSG_ID_COUNCILR    0x0037/*u8 winner: 0=none, 1,2=candidate*/
#define GAME_MSG_ID_COUNCILA    0x0038/*u8 refuse: 0=accept, 1=refuse*/
#define GAME_MSG_ID_COUNCILE    0x0039/*u8 winner: 0=none, 1,2=candidate, u8 refused: 0/1*/
#define GAME_MSG_ID_END         0x003a/*u8 winner, u8 end_type*/

#endif
