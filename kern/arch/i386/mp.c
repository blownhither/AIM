#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/param.h>
#include <proc.h>


struct cpu cpus[NCPU];
int ismp;
int ncpu;
uchar ioapicid;
