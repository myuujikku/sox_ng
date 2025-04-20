/* ErrMsg.h */

#include <stdint.h>

extern int ErrMsgFlg;

extern void ErrMsgFlagError(char *ErrMsg);
extern void ErrMsgWarning(char *WrnMsg);
extern char *ErrMsgNumToStr(char *Result, int64_t InVal);
