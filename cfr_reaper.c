//# vim: ts=2 sw=2 et :

// This code does not maintain a family structure as it does not even bother to store status codes.
// All it does it listen for the appropriate signal when any child dies.
// It would make a very poor substitute for init outside a container.

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include <syslog.h>
#include <string.h>
#include <malloc.h>
#include <sys/file.h>

#define MAX_ARGS 512
#define MAX_ARG_LEN 255

#define DUMB_ID_METHOD

static void chld_handler (
      const int sig) {

  pid_t pid;
  int st;

  int saved_errno = errno;
  while((pid = waitpid(-1, &st, WNOHANG)) != 0) {
    if (errno == ECHILD) break;
    syslog(LOG_INFO, "reaping child: `%d'.", pid);
  }
  errno = saved_errno;
}

void conf_sig(
      int sig_num,
      struct sigaction action,
      void(*fn)(int)) {

  memset(&action, 0, sizeof(action));
  action.sa_handler = fn;
  action.sa_flags = SA_RESTART;
  sigemptyset(&action.sa_mask);
  sigaction(sig_num, &action, NULL);
}

void exec_child(
      char** argv,
      char** env,
      struct sigaction* action) {

  pid_t pid;
  sigset_t nmask, omask;
  int sig_num;

  // Block SIGCHLD, SIGINT for fork
  sigemptyset(&nmask); sigaddset(&nmask, SIGCHLD); sigaddset(&nmask, SIGINT); sigprocmask(SIG_BLOCK, &nmask, &omask);

  // Run desired top process
  // Note: even though we are potentially starting a daemon,
  // no need to double fork as the end result would be the same:
  // the daemon being attached to 1.
  if ((pid = fork()) < 0) { // Error
    sigprocmask(SIG_SETMASK, &omask, NULL);
    syslog(LOG_ERR, "fork error -- return code: %d", pid);
  }
  else if(pid == 0) { // Child
    sigprocmask(SIG_SETMASK, &omask, NULL);

    for(sig_num=0; sig_num<=NSIG; sig_num++) {
      conf_sig(sig_num, *action, SIG_DFL);
    }
      
    int ex_ret = execve(argv[1], argv+1, env);

    // Why am I still here???
    syslog(LOG_ERR, "fork+execve error -- %d", ex_ret);
  }
  else { // Parent, obviously
    sigprocmask(SIG_SETMASK, &omask, NULL);
    // This would block: chld_handler(0 /* bogus signal */);
  }
}

int main(
      int argc,
      char** argv,
      char** envp) {

  struct sigaction action;
  FILE *fp;
  char buf[MAX_ARG_LEN+1];
  char *new_args[MAX_ARGS+1];
  char **argv2;
  char *env2[] = { NULL };
  int line_ctr = 0;
  char* pos;
  
  // The first time we are run, we are pretending to be the original executable but will in fact
  // also perform orphaned process reaping.
  // The second time, we are assuming that we are really meant to act _as_ the original executable.
  #ifdef DUMB_ID_METHOD
  int act_as_init = (1 == getpid()); // Duh
  #else
  // Detect which scenario this is
  // This version allows us to test this code without actually being process 1
  fp = fopen("/var/run/cfrinit.pid", "w");
  int act_as_init = !flock(fileno(fp), LOCK_EX|LOCK_NB);
  fprintf(fp, "%d", getpid());
  fclose(fp);
  #endif
  
  // Arguments can come from the command line or, more likely
  // for existing containers, from a configuration file called `cfr_reaper_args`
  // which can be in the start directory or in /etc.
  // This file contains what will end up being argv[1]..argv[n], one arg per line.
  if(argc < 2) {
    fp = fopen("cfr_reaper_args", "r");
    if(!fp) {
      fp = fopen("/etc/cfr_reaper_args", "r");
      if(!fp) {
        syslog(LOG_ERR, "wrong number of arguments for cfr_reaper: %d, expected 1 or more", argc);
        return -1;
      }
    }
    argc = 1;
    while (fgets(buf, sizeof(buf)-1, fp) && !feof(fp) && argc<(MAX_ARGS-2)) {
      char *new_arg = (char*)malloc((MAX_ARG_LEN+1)*sizeof(char*));
      strncpy(new_arg, buf, MAX_ARG_LEN);
      if((pos = strchr(new_arg, '\n')))
          *pos = '\0';
      new_args[argc++] = new_arg;
    }
    fclose(fp);
    new_args[argc] = NULL;
    argv2 = (char**)&new_args;
  }
  else {
    argv2 = argv;
  }

  // Please, report children's disappearance
  if(act_as_init) {
    conf_sig(SIGCHLD, action, chld_handler);

    // This function can be called as many times as needed, therefore it would be
    // perfectly possible to add a separator in the config file and use that as a
    // hint to execute multiple commands.
    exec_child(argv2, env2, &action);

    // Start infinite loop
    for(;;)
      sched_yield();
  }
  else {
    fp = fopen("cfr_reaper_actual", "r");
    if(!fp) {
      fp = fopen("/etc/cfr_reaper_actual", "r");
      if(!fp) {
        syslog(LOG_ERR, "attempting to run %s as actual binary without cfr_reaper_actual defined.", argv[0]);
        return -1;
      }
    }
    if(!fgets(buf, sizeof(buf)-1, fp)) {
      return -1;
    }
    if((pos = strchr(buf, '\n')))
        *pos = '\0';
    fclose(fp);
    *argv = strdup(buf);
    int ex_ret = execve(buf, argv, envp);

    // Why am I still here???
    syslog(LOG_ERR, "execve error -- %d", ex_ret);
  }

  // Nope. Not happening.
}

