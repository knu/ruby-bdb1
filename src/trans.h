#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DB_SET_RANGE    R_CURSOR
#define DB_FIRST        R_FIRST
#define DB_AFTER        R_IAFTER
#define DB_BEFORE       R_IBEFORE
#define DB_LAST         R_LAST
#define DB_NEXT         R_NEXT
#define DB_NOOVERWRITE  R_NOOVERWRITE
#define DB_PREV         R_PREV
#define DB_FIXEDLEN     R_FIXEDLEN
#define DB_DUP          R_DUP

#define DB_KEYEXIST 1
#define DB_NOTFOUND 1

#define DB_CREATE O_CREAT
#define DB_WRITE O_RDWR
#define DB_RDONLY O_RDONLY
#define DB_TRUNCATE O_TRUNC

