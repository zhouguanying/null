#ifndef _LOG_Dbg_H_H
#define _LOG_Dbg_H_H

#define log_warning(...) do { printf("##### %s %6d %s: ", __FILE__, __LINE__, __func__); printf( __VA_ARGS__ ); } while (0)

#define log_debug(...) do { printf("====> %s %6d %s: ", __FILE__, __LINE__, __func__); printf( __VA_ARGS__ ); } while (0)

#endif
