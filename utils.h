#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>      /* Type definitions used by many programs */               
#include <stdio.h>          /* Standard I/O functions */                               
#include <stdlib.h>         /* Prototypes of commonly used library functions,
                                plus EXIT_SUCCESS and EXIT_FAILURE constants */         
#include <unistd.h>         /* Prototypes for many system calls */                     
#include <errno.h>          /* Declares errno and defines error constants */           
#include <string.h>         /* Commonly used string-handling functions */              

#include "error_handlers.h" /* Desclares error handling functions */


#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

typedef enum { FALSE, TRUE } Boolean;

#define min(m,n) ((m) < (n) ? (m) : (n))                                        
#define max(m,n) ((m) > (n) ? (m) : (n)) 

#endif
