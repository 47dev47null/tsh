#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include "utils.h"
#include "queue.h"

#define PIPE        '|'
#define REDIR_IN    '<'
#define REDIR_OUT   '>'
#define INITSIZE    8               /* initial argv size in process */

/* A process is a single process. */
typedef struct process
{
    char **argv;                    /* for execvp */
    SIMPLEQ_ENTRY(process) next;    /* next process in pipeline */ 
} process;

/* A job is a pipeline of processes. */
typedef struct job
{
    char *cmd;                      /* origin command, used for messages */
    SIMPLEQ_HEAD(, process) head;   /* list of processes in this job */
    int in, out;                    /* standard I/O channels */
} job;

job *j = NULL;

void init_shell(void);
int create_job(void);
int create_process(char *sp, char *ep);
void launch_job(int bg);
void launch_process(process *p, int infile, int outfile, int bg);
void teardown_job(void);

void init_shell(void)
{
    /* ignore Ctrl-C */
    if (signal(SIGINT, SIG_IGN) == SIG_ERR)
        errExit("signal");

    /* print welcome */
    printf("Tiny Shell v1.0 Created by Yanzhe Chen, 2013.03\n");
    printf("License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n");
    printf("This is free software: you are free to change and redistribute it.\n");
    printf("There is NO WARRANTY, to the extent permitted by law.\n");
    printf("For bug reporting, please email: <0x2fdev0x2fnull@gmail.com>\n");
}

void teardown_job(void)
{
    if (j != NULL)
    {
        if (j->cmd != NULL)
            free(j->cmd);

        process *p;
        while (!SIMPLEQ_EMPTY(&j->head))
        {
            p = SIMPLEQ_FIRST(&j->head);
            SIMPLEQ_REMOVE_HEAD(&j->head, next);
            free(p->argv);
            free(p);
        }

        free(j);
    }
}

void launch_process(process *p, int infile, int outfile, int bg)
{
    /* set the standard I/O channels of the new process */
    if (infile != STDIN_FILENO)
    {
        if (dup2(infile, STDIN_FILENO) != STDIN_FILENO)
            errExit("dup2");
        close(infile);
    }
    if (outfile != STDOUT_FILENO)
    {
        if (dup2(outfile, STDOUT_FILENO) != STDOUT_FILENO)
            errExit("dup2");
        close(outfile);
    }

    /* execute the new process */
    execvp(p->argv[0], p->argv);
    errExit("execvp");
}


void launch_job(int bg)
{
    process *p;
    pid_t pid;
    int mypipe[2], infile, outfile;

    infile = j->in;
    SIMPLEQ_FOREACH(p, &j->head, next)
    {
        /* set up pipes if necessary */ 
        if (SIMPLEQ_NEXT(p, next))
        {
            if (pipe(mypipe) == -1)
                errExit("pipe");
            outfile = mypipe[1];
        }
        else
            outfile = j->out;

        /* fork the child process */
        switch(pid = fork())
        {
        case -1:
            errExit("fork");

        case 0:
            /* child process */
            launch_process(p, infile, outfile, bg);
            break;

        default:
            /* parent process */
            if (waitpid(pid, NULL, 0) == -1)
                errExit("waitpid");
            break;
        }

        /* clean up after pipes */
        if (infile != STDIN_FILENO)
            close(infile);
        if (outfile != STDOUT_FILENO)
            close(outfile);

        infile = mypipe[0];
    }
}

int create_process(char *sp, char *ep)
{
    char *sa, *ea;
    process *p;
    size_t argc, size;

    sa = sp;
    ea = NULL;
    argc = 0;
    size = INITSIZE;                /* default size of argc */

    if ((p = (process *) malloc(sizeof(process))) == NULL)
        errExit("malloc");

    if ((p->argv = (char **) malloc((size + 1) * sizeof(char *))) == NULL)
        errExit("malloc");
    p->argv[size] = NULL;

    while (sa < ep)
    {
        while (sa < ep && isspace(*sa))
            sa++;
        for (ea = sa + 1; ea < ep && !isspace(*ea); ea++)
            ;
        if (sa < ep)
        {
            if (argc >= size)       /* enlarge argv */
            {
                size *= 2;
                p->argv = (char **) realloc(p->argv, (size + 1) * sizeof(char *));
                if (p->argv == NULL)
                    errExit("malloc");
                p->argv[size] = NULL;
            }

            p->argv[argc++] = sa;
            *ea = '\0';
        }
        sa = ea + 1;
    }
    p->argv[argc] = NULL;

    if (argc == 0)
    {
        free(p->argv);
        free(p);
        return -1;
    }
    else
        SIMPLEQ_INSERT_TAIL(&j->head, p, next); 
    return 0;
}

int create_job(void)
{
    size_t len = 0;
    ssize_t read;
    char *sp, *ep;
    char *r, *f, *sf, *ef; 

    j = (job *) malloc(sizeof(job));
    if (j == NULL)
        errExit("malloc");
    SIMPLEQ_INIT(&j->head);
    j->in = STDIN_FILENO;
    j->out = STDOUT_FILENO;

    if ((read = getline(&j->cmd, &len, stdin)) == -1)
        fatal("getline");

#ifdef DEBUG
    printf("%-30s: %s", "original cmd", j->cmd);
#endif

    if (j->cmd[0] == 'c' && j->cmd[1] == 'd' && isspace(j->cmd[2]))
    {
        j->cmd[read-1] = '\0';      /* chop ending '\n' */
        if (chdir(j->cmd+3) == -1)
            errExit("chdir");
        return -1;
    }
    
    if (j->cmd[0] == 'q' && isspace(j->cmd[1]))
        return 1;                   /* signal to exit */

    sp = j->cmd;
    ep = NULL;
    r = sf = ef = NULL;

    r = strchr(sp, REDIR_IN);
    ep = strchr(sp, PIPE);
    if (r && (!ep || r < ep))
    {
        for (sf = r + 1; *sf != '\0' && isspace(*sf); sf++)
            ;
        for (ef = sf; *ef != '\0' && !isspace(*ef); ef++)
            ;
        if (*sf == '\0')
        {
            errMsg("missing filename after '<'");
            return -1;
        }
        len = ef - sf;
        f = (char *) malloc((len + 1) * sizeof(char));
        if (f == NULL)
            errMsg("malloc");
        strncpy(f, sf, len);
        f[len] = '\0';

        if ((j->in = open(f, O_RDONLY)) == -1)
            errExit("open");

#ifdef DEBUG
        printf("%-30s: %s\n", "input file", f);
#endif
        free(f);
        for(; r < ef; r++)          /* erase input redirect from cmd */
            *r = ' ';
    }

    r = strchr(sp, REDIR_OUT);
    ep = strrchr(sp, PIPE);
    if (r && (!ep || ep < r))
    {
        for (sf = r + 1; *sf != '\0' && isspace(*sf); sf++)
            ;
        for (ef = sf; *ef != '\0' && !isspace(*ef); ef++)
            ;
        if (*sf == '\0')
        {
            errMsg("missing filename after '>'");
            return -1;
        }
        len = ef - sf;
        f = (char *) malloc((len + 1) * sizeof(char));
        if (f == NULL)
            errMsg("malloc");
        strncpy(f, sf, len);
        f[len] = '\0';
        if ((j->out = open(f, O_WRONLY | O_TRUNC | O_CREAT,
                    S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR)) == -1)
            errExit("open");
        
#ifdef DEBUG
        printf("%-30s: %s\n", "output file", f);
#endif
        free(f);
        for(; r < ef; r++)          /* erase output redirect from cmd */
            *r = ' ';
    }

#ifdef DEBUG
    printf("%-30s: %s", "erased I/O redirect", sp);
#endif

    while ((ep = strchr(sp, PIPE)) != NULL)
    {
        if (create_process(sp, ep) == -1)
        {
            errMsg("empty pipeline");
            return -1;
        }
        else
        {
            read -= ep - sp + 1;
            sp = ep + 1;
        }
    }

    if (read > 0)
    {
        if (create_process(sp, sp + strlen(sp)) == -1)
        {
            errMsg("empty command");
            return -1;
        }
    }

#ifdef DEBUG
    printf("*** TEAR DOWN ***\n");
    process *p;
    SIMPLEQ_FOREACH(p, &j->head, next)
    {
        size_t i;
        for (i = 0; p->argv[i]; i++)
            printf("%s\t", p->argv[i]);
        printf("\n");
    }
    printf("*** TEAR DOWN ***\n");
#endif

    return 0;
}

int main(void)
{
    int rc;

    init_shell();
    while (1)
    {
        printf("(tsh) ");
        rc = create_job();
        if(rc == 0)
        {
            launch_job(0);
            teardown_job();
        }
        else if (rc == 1)
        {
            teardown_job();
            break;
        }
        else if (rc == -1)
        {
            teardown_job();
            continue;
        }
    }
    return EXIT_SUCCESS;
}
