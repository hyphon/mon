#include <mntent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "SocketHttp.h"
#include "NodeStatus.h"
#include "NodeResourceStatus.h"

void proc_read(struct proc *proc, int num)
{
int i=0;
int size=0;
int found=0;

  if(proc[num].fp == NULL) {
    if( (proc[num].fp = fopen(proc[num].filename,"r")) == NULL) {
      proc[num].fp = NULL;

      perror("The following error occurred");
      fprintf(stderr, "failed to open file %s", proc[num].filename);
      exit(1);
    }
  }

  rewind(proc[num].fp);

  if(proc[num].size == 0) {
    /* first time so allocate  initial now */
    proc[num].buf = malloc(2048);
    proc[num].size = 2048;
  }
  
  for(i=0;i<1024;i++) {
    size = fread(proc[num].buf, 1, proc[num].size-1, proc[num].fp);
    if(size < proc[num].size -1)
      break;
    proc[num].size +=1024;
    proc[num].buf = realloc(proc[num].buf,proc[num].size);
    rewind(proc[num].fp);
if (debugl >= 3) {
    printf("\n\n+++++++++++++++++++++++++++++++++++++++++++++++1\n\n");
}
  }

  proc[num].buf[size]=0;
  proc[num].lines=0;
  proc[num].line[0]=&proc[num].buf[0];
  if(num == P_VERSION) {
    found=0;
    for(i=0;i<size;i++) { /* remove some weird stuff found the hard way in various Linux versions and device drivers */
      /* place ") (" on two lines */
      if( found== 0 &&
      proc[num].buf[i]   == ')' &&
      proc[num].buf[i+1] == ' ' &&
      proc[num].buf[i+2] == '(' ) {
        proc[num].buf[i+1] = '\n';
        found=1;
      } else {
        /* place ") #" on two lines */
        if( proc[num].buf[i]   == ')' &&
        proc[num].buf[i+1] == ' ' &&
        proc[num].buf[i+2] == '#' ) {
          proc[num].buf[i+1] = '\n';
          found=1;
        }
        /* place " #1" on two lines */
        if(
        proc[num].buf[i]   == ' ' &&
        proc[num].buf[i+1] == '#' &&
        proc[num].buf[i+2] == '1' ) {
          proc[num].buf[i] = '\n';
          found=1;
        }
      }
    }
  }
  for(i=0;i<size;i++) {
    /* replace Tab characters with space */
    if(proc[num].buf[i] == '\t')  {
      proc[num].buf[i]= ' '; 
    }
    else if(proc[num].buf[i] == '\n') {
      /* replace newline characters with null */
      proc[num].lines++;
      proc[num].buf[i] = '\0';
      proc[num].line[proc[num].lines] = &proc[num].buf[i+1];
    }
    if(proc[num].lines==PROC_MAXLINES-1)
      break;
  }

  fclose( proc[num].fp);
  proc[num].fp = NULL;
}

void proc_cpu(struct cpu_data *data,struct proc * proc)
{
int i=0;
int row=0;
static int proc_cpu_first_time = 1;
unsigned long long user=0;
unsigned long long nice=0;
unsigned long long sys=0;
unsigned long long idle=0;
unsigned long long iowait=0;
unsigned long long hardirq=0;
unsigned long long softirq=0;
unsigned long long steal=0;
static int stat8 = 0; /* used to determine the number of variables on a line */
int cpus = data->cpus;
struct cpu_stats *p = data->p;

  if(proc_cpu_first_time) {
    stat8 = sscanf(&proc[P_STAT].line[0][5], "%lld %lld %lld %lld %lld %lld %lld %lld", 
      &user,
      &nice,
      &sys,
      &idle,
      &iowait,
      &hardirq,
      &softirq,
      &steal);
    proc_cpu_first_time = 0;
  }
  user = nice = sys = idle = iowait = hardirq = softirq = steal = 0;
  if(stat8 == 8) {
    sscanf(&proc[P_STAT].line[0][5], "%lld %lld %lld %lld %lld %lld %lld %lld", 
      &user,
      &nice,
      &sys,
      &idle,
      &iowait,
      &hardirq,
      &softirq,
      &steal);
  } else { /* stat 4 variables here as older Linux proc */
    sscanf(&proc[P_STAT].line[0][5], "%lld %lld %lld %lld", 
      &user,
      &nice,
      &sys,
      &idle);
  }
  p->cpu_total.user = user + nice;
  p->cpu_total.wait = iowait; /* in the case of 4 variables = 0 */
  p->cpu_total.sys  = sys;
  /* p->cpu_total.sys  = sys + hardirq + softirq + steal;*/
  p->cpu_total.idle = idle;
  
  p->cpu_total.irq     = hardirq;
  p->cpu_total.softirq = softirq;
  p->cpu_total.steal   = steal;
  p->cpu_total.nice    = nice;

  for(i=0;i<cpus;i++ ) {
      user = nice = sys = idle = iowait = hardirq = softirq = steal = 0;

      /* allow for large CPU numbers */
      if(i+1 > 1000)     row = 8;
      else if(i+1 > 100) row = 7;
      else if(i+1 > 10)  row = 6;
      else row = 5;

      if(stat8 == 8) {
    sscanf(&proc[P_STAT].line[i+1][row], 
      "%lld %lld %lld %lld %lld %lld %lld %lld", 
    &user,
    &nice,
    &sys,
    &idle,
    &iowait,
    &hardirq,
    &softirq,
    &steal);
      } else {
    sscanf(&proc[P_STAT].line[i+1][row], "%lld %lld %lld %lld", 
    &user,
    &nice,
    &sys,
    &idle);
      }
    p->cpuN[i].user = user + nice;
    p->cpuN[i].wait = iowait;
    p->cpuN[i].sys  = sys;
    /*p->cpuN[i].sys  = sys + hardirq + softirq + steal;*/
    p->cpuN[i].idle = idle;

    p->cpuN[i].irq     = hardirq;
    p->cpuN[i].softirq = softirq;
    p->cpuN[i].steal   = steal;
    p->cpuN[i].nice    = nice;
  }
}

void proc_diskstats(struct dsk_data *data)
{
static FILE *fp = NULL;
char buf[1024]={0};
int i=0;
int ret=0;
struct dsk_stats *p = data->p;

  if( fp == NULL) {
    if( (fp = fopen("/proc/diskstats","r")) == NULL) {
      data->disks=0;

      perror("failed to open - /proc/diskstats");
      exit(1);
    }
  }
/*
   2    0 fd0 1 0 2 13491 0 0 0 0 0 13491 13491
   3    0 hda 41159 53633 1102978 620181 39342 67538 857108 4042631 0 289150 4668250
   3    1 hda1 58209 58218 0 0
   3    2 hda2 148 4794 10 20
   3    3 hda3 65 520 0 0
   3    4 hda4 35943 1036092 107136 857088
  22    0 hdc 167 5394 22308 32250 0 0 0 0 0 22671 32250 <-- USB !!
   8    0 sda 990 2325 4764 6860 9 3 12 417 0 6003 7277
   8    1 sda1 3264 4356 12 12
*/
  rewind(fp);

  for(i=0;i<DISKMAX;) {
    if(fgets(buf,sizeof(buf),fp) == NULL)
      break;
    /* zero the data ready for reading */
    memset(p->dk+i,0,sizeof(struct dsk_stat));

    ret = sscanf(&buf[0], "%d %d %s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
      &p->dk[i].dk_major,
      &p->dk[i].dk_minor,
      &p->dk[i].dk_name[0],
      &p->dk[i].dk_reads,
      &p->dk[i].dk_rmerge,
      &p->dk[i].dk_rkb,
      &p->dk[i].dk_rmsec,
      &p->dk[i].dk_writes,
      &p->dk[i].dk_wmerge,
      &p->dk[i].dk_wkb,
      &p->dk[i].dk_wmsec,
      &p->dk[i].dk_inflight,
      &p->dk[i].dk_time,
      &p->dk[i].dk_11 );
    if(ret == 7) { /* shuffle the data around due to missing columns for partitions */
      p->dk[i].dk_partition = 1;
      p->dk[i].dk_wkb = p->dk[i].dk_rmsec;
      p->dk[i].dk_writes = p->dk[i].dk_rkb;
      p->dk[i].dk_rkb = p->dk[i].dk_rmerge;
      p->dk[i].dk_rmsec=0;
      p->dk[i].dk_rmerge=0;
  
    }
    else if(ret == 14) p->dk[i].dk_partition = 0;
    else {
      fprintf(stderr,"disk sscanf wanted 14 but returned=%d line=%s\n", 
        ret,buf);
      exit(1);
    }

    p->dk[i].dk_rkb /= 2; /* sectors = 512 bytes */
    p->dk[i].dk_wkb /= 2;
    p->dk[i].dk_xfers = p->dk[i].dk_reads + p->dk[i].dk_writes;
    if(p->dk[i].dk_xfers == 0)
      p->dk[i].dk_bsize = 0;
    else
      p->dk[i].dk_bsize = ((p->dk[i].dk_rkb+p->dk[i].dk_wkb)/p->dk[i].dk_xfers)*1024;

    p->dk[i].dk_time /= 10.0; /* in milli-seconds to make it upto 100%, 1000/100 = 10 */
  
    if( p->dk[i].dk_xfers > 0)
      i++;  
  }

  fclose(fp);
  fp = NULL;

  data->disks = i;
}

void strip_spaces(char *s)
{
char *p=NULL;
int spaced=1;

  p=s;
  for(p=s;*p!=0;p++) {
    if(*p == ':')
      *p=' ';
    if(*p != ' ') {
      *s=*p;
      s++;
      spaced=0;
    } else if(spaced) {
      /* do no thing as this is second space */
      } else {
        *s=*p;
        s++;
        spaced=1;
      }

  }
  *s = 0;
}

void proc_partitions(struct dsk_data *data)
{
static FILE *fp = NULL;
char buf[1024]={0};
int i = 0;
int ret=0;
struct dsk_stats *p = data->p;

  if( fp == NULL) {
    if( (fp = fopen("/proc/partitions","r")) == NULL) {

      perror("failed to open - /proc/partitions");
      exit(1);
    }
  }

  rewind(fp);

  if(fgets(buf,sizeof(buf),fp) == NULL) {
    perror("fgets()");
    exit(1);
  }

  if(fgets(buf,sizeof(buf),fp) == NULL) {
    perror("fgets()");
    exit(1);
  }
/*
major minor  #blocks  name     rio rmerge rsect ruse wio wmerge wsect wuse running use aveq

  33     0    1052352 hde 2855 15 2890 4760 0 0 0 0 -4 7902400 11345292
  33     1    1050304 hde1 2850 0 2850 3930 0 0 0 0 0 3930 3930
   3     0   39070080 hda 9287 19942 226517 90620 8434 25707 235554 425790 -12 7954830 33997658
   3     1   31744408 hda1 651 90 5297 2030 0 0 0 0 0 2030 2030
   3     2    6138720 hda2 7808 19561 218922 79430 7299 20529 222872 241980 0 59950 321410
   3     3     771120 hda3 13 41 168 80 0 0 0 0 0 80 80
   3     4          1 hda4 0 0 0 0 0 0 0 0 0 0 0
   3     5     408208 hda5 812 241 2106 9040 1135 5178 12682 183810 0 11230 192850
*/
  for(i=0;i<DISKMAX;i++) {
    if(fgets(buf,sizeof(buf),fp) == NULL)
      break;
    strip_spaces(buf);
    ret = sscanf(&buf[0], "%d %d %llu %s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
      &p->dk[i].dk_major,
      &p->dk[i].dk_minor,
      &p->dk[i].dk_blocks,
    (char *)&p->dk[i].dk_name,
      &p->dk[i].dk_reads,
      &p->dk[i].dk_rmerge,
      &p->dk[i].dk_rkb,
      &p->dk[i].dk_rmsec,
      &p->dk[i].dk_writes,
      &p->dk[i].dk_wmerge,
      &p->dk[i].dk_wkb,
      &p->dk[i].dk_wmsec,
      &p->dk[i].dk_inflight,
      &p->dk[i].dk_use,
      &p->dk[i].dk_aveq
      );
    p->dk[i].dk_rkb /= 2; /* sectors = 512 bytes */
    p->dk[i].dk_wkb /= 2;
    p->dk[i].dk_xfers = p->dk[i].dk_rkb + p->dk[i].dk_wkb;
    if(p->dk[i].dk_xfers == 0)
      p->dk[i].dk_bsize = 0;
    else
      p->dk[i].dk_bsize = (p->dk[i].dk_rkb+p->dk[i].dk_wkb)/p->dk[i].dk_xfers*1024;

    p->dk[i].dk_time /= 10.0; /* in milli-seconds to make it upto 100%, 1000/100 = 10 */
  
    if(ret != 15) {
      fprintf(stderr,"sscanf wanted 15 returned = %d line=%s\n", ret,buf);
      exit(1);
    }
  }

  fclose(fp);
  fp = NULL;

  data->disks = i;
}

void proc_disk(struct dsk_data *data)
{
struct stat buf={0};
int ret=-1;

  if(data->disk_mode == 0) {
    ret = stat("/proc/diskstats", &buf);
    if(ret != -1) {
      data->disk_mode=DISK_MODE_DISKSTATS;
    } else {
      ret = stat("/proc/partitions", &buf);
      if(ret != -1) {
        data->disk_mode=DISK_MODE_PARTITIONS;
      } else {
        data->disk_mode=DISK_MODE_UNKNOWN;
      }
    }
  }

  switch(data->disk_mode){
    case DISK_MODE_DISKSTATS:
      proc_diskstats(data); break;
    case DISK_MODE_PARTITIONS:
      proc_partitions(data); break;
    default:
      fprintf(stderr,"ERROR: wrong data->disk_mode %d\n",data->disk_mode);
      exit(1);
  }
}

void proc_init(struct proc * proc)
{
  proc[P_CPUINFO].filename = "/proc/cpuinfo";
  proc[P_STAT].filename    = "/proc/stat";
  proc[P_VERSION].filename = "/proc/version";
  proc[P_MEMINFO].filename = "/proc/meminfo";
  proc[P_UPTIME].filename  = "/proc/uptime";
  proc[P_LOADAVG].filename = "/proc/loadavg";
  proc[P_NFS].filename     = "/proc/net/rpc/nfs";
  proc[P_NFSD].filename    = "/proc/net/rpc/nfsd";
  proc[P_VMSTAT].filename   = "/proc/vmstat";
}

/* Convert secs + micro secs to a double */
double  doubletime(void)
{
  struct timeval tv={0};
  struct timezone tz={0};

  gettimeofday(&tv,&tz);
  return((double)(tv.tv_sec-tz.tz_minuteswest*60) + tv.tv_usec * 1.0e-6);
}

int jfs_load(struct jfs* jfs,int load)
{
int i=0;
struct stat stat_buffer={0};
static FILE * mfp = NULL; /* FILE pointer for mtab file*/
struct mntent *mp=NULL; /* mnt point stats */
//static int jfs_loaded = 0;
int jfses = 0;

  if(load==LOAD) { 
    if(mfp == NULL) { 
      mfp = setmntent("/etc/mtab","r");
    }

    rewind(mfp);

    for(i=0; i<JFSMAX && ((mp = getmntent(mfp) ) != NULL); i++) {
      strncpy(jfs[i].device, mp->mnt_fsname,JFSNAMELEN);
      strncpy(jfs[i].name,mp->mnt_dir,JFSNAMELEN);
      strncpy(jfs[i].type, mp->mnt_type,JFSTYPELEN);
      mp->mnt_fsname[JFSNAMELEN-1]=0;
      mp->mnt_dir[JFSNAMELEN-1]=0;
      mp->mnt_type[JFSTYPELEN-1]=0;
    }
    //}

    //jfs_loaded = 1;
    jfses=i;

    /* 1st or later time - just reopen the mount points */
    for(i=0;i<JFSMAX && (jfs[i].name[0] !=0);i++) {
      if(stat(jfs[i].name, &stat_buffer) != -1 ) {
        jfs[i].fd = open(jfs[i].name, O_RDONLY);
        if(jfs[i].fd != -1 ) 
                jfs[i].mounted = 1;
        else
                jfs[i].mounted = 0;
      }
      else jfs[i].mounted = 0;
    }
  }
  else { /* this is an unload request */
    if(mfp != NULL) {
      for(i=0;i<JFSMAX && jfs[i].name[0] != 0;i++) {
        if(jfs[i].mounted) {
          close(jfs[i].fd);
          jfs[i].fd=-1;
        }
      }
    }
    else {
    }
      /* do nothing */ ;
  }

  //endmntent(mfp);

  return jfses;
}

long long proc_mem_search( char *s,struct proc* proc)
{
int i=0, j=0;
int len=0;
unsigned long long size=0;
  len=strlen(s);
  for(i=0;i<proc[P_MEMINFO].lines;i++ ) {
    if( strncmp(s, proc[P_MEMINFO].line[i],len) ) {
      continue;
    }

    for(j=len;
      proc[P_MEMINFO].line[i][j] != 0;
      j++) {

      //ignore non-digital character
      if(!isdigit(proc[P_MEMINFO].line[i][j])) {
        continue;
      }

      size = atoll( &proc[P_MEMINFO].line[i][j]);
      break;
    }

    if(proc[P_MEMINFO].line[i][j] == 0) {
      fprintf(stderr,"ERROR:can't find digital from %s\n",proc[P_MEMINFO].line[i]);
      exit(1);
    }

    return size;
  }

  if(i>= proc[P_MEMINFO].lines) {
    fprintf(stderr,"ERROR:can't find memory %s from %s\n",s,proc[P_MEMINFO].filename);
    exit(1);
  }

  return -1;
}

void proc_mem(struct mem_stats *p, struct proc * proc)
{
  p->mem_stat.memtotal   = proc_mem_search("MemTotal",proc);
  p->mem_stat.memfree    = proc_mem_search("MemFree",proc);
  p->mem_stat.buffers    = proc_mem_search("Buffers",proc);
  p->mem_stat.cached     = proc_mem_search("Cached",proc);
}

void socket_net(struct net_data *data)
{
    int i=0, j=0;
    int fd=-1;         //socket fd
    int count=0;
    struct ifconf ifc={0};
    struct ifreq req[NETMAX];
    struct ethtool_cmd  cmd={0};

    struct net_stats *p = data->p;
    struct net_param *pa=(struct net_param *)&data->net_param;
  
    FILE *fd_slaves = NULL;
    char fn_slaves[128]={0};
    char buf[1024] = {0};
    char delims[] = " ";
    char *result = NULL;

    //if(ifc.ifc_len != 0) goto init_nic_end;

    ifc.ifc_len = NETMAX*sizeof(struct ifreq);
    memset(req, 0, ifc.ifc_len);
    ifc.ifc_req = req;

    //create socket fd
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
      perror("The following error occurred");
      exit(1);
    }

    //get nics
    if (ioctl(fd, SIOCGIFCONF, &ifc) < 0)
    {
      perror("The following error occurred");
      exit(1);
    }

    count = ifc.ifc_len/sizeof(struct ifreq);
    data->nets = count;


    for (i=0; i<data->nets; i++)
    {

//      for(j=0;j<networks;j++) {
//        if(!strcmp(p->ifnets[j].if_name,ifc.ifc_req[i].ifr_name)) break;
//      }
//
//      if(j>=networks) {
//        fprintf(stderr,"can't find NIC %s in /proc/net/dev\n",ifc.ifc_req[i].ifr_name);
//        exit(1);
//      }

      strcpy(pa[i].name,ifc.ifc_req[i].ifr_name);

      if(ioctl(fd,SIOCGIFADDR,ifc.ifc_req+i)<0){
        perror("ioctl error");
        exit(1);
      }       
      else {
        strcpy(pa[i].ip, inet_ntoa(((struct sockaddr_in*)&((ifc.ifc_req+i)->ifr_addr))->sin_addr));
      }

      if(ioctl(fd, SIOCGIFFLAGS, ifc.ifc_req+i) < 0) {
        perror("The following error occurred");
        exit(1);
      }
      else {
        pa[i].flags = (ifc.ifc_req+i)->ifr_flags;
      }

      //if it's MASTER, don't call ethtool to get bandwidth
      if(pa[i].flags & IFF_MASTER) {
        continue;
      }

      (ifc.ifc_req+i)->ifr_data = (void *)&cmd;
      cmd.cmd = ETHTOOL_GSET; /* "Get settings" */

      if (ioctl(fd, SIOCETHTOOL, ifc.ifc_req+i) == -1) {
        /* Unknown */
        //p->ifnets[i].bandwidth = -1L;
        //p->ifnets[i].duplex = DUPLEX_UNKNOWN;
        //perror("socket");
        //exit(EXIT_FAILURE);
if (debugl >= 2) {
        perror("ioctl SIOCETHTOOL");
        fprintf(stderr,"WARNING: ethtool cant report bandwidth/duplex of NIC %s\n",p->ifnets[i].if_name);
}

        pa[i].bandwidth = -1L;
        pa[i].duplex= DUPLEX_UNKNOWN;
      } else {
        pa[i].bandwidth = ethtool_cmd_speed(&cmd);
        pa[i].duplex= cmd.duplex;
      }
    }

    for (i=0; i<data->nets; i++)
    {
      if(pa[i].flags & IFF_MASTER) {
        strcpy(fn_slaves, "/sys/class/net/");
        strcat(fn_slaves, pa[i].name);
        strcat(fn_slaves, "/bonding/slaves");

        fd_slaves = fopen(fn_slaves, "r");

        if (fd_slaves == NULL) {
            perror("Can't open /sys/class/net/xx/bonding/slaves for bond interface");
            exit(1);
        }

        if(fgets(buf, sizeof(buf), fd_slaves) == NULL) {
          perror("fgets()");
          exit(1);
        }

        result = strtok(buf, delims);

        while (result != NULL) {
          for(j=0;j<data->nets;j++) {
            if(!strcmp(result, pa[j].name)) break;
          }

          if(j >= data->nets) {
            fprintf(stderr,"ERROR: can't find nic %s\n",result);
            exit(1);
          }

          pa[i].bandwidth += pa[j].bandwidth;
          result = strtok(NULL, delims);
        }

      fclose(fd_slaves);
      fd_slaves = NULL;
      }
    }

    close(fd);
}

void proc_net(struct net_data *data)
{
static FILE *fp = NULL;
char buf[1024]={0};
int i=0;
int ret=0;
unsigned long long junk=0;
struct net_stats *p = data->p;

  if( fp == NULL) {
    if( (fp = fopen("/proc/net/dev","r")) == NULL) {
      data->networks=0;

      perror("failed to open - /proc/net/dev");
      exit(1);
    }
  }

  rewind(fp);

  if(fgets(buf,sizeof(buf),fp) == NULL)  /* throw away the header lines */
  {
    perror("fgets()");
    exit(1);
  }
  if(fgets(buf,sizeof(buf),fp) == NULL)  /* throw away the header lines */
  {
    perror("fgets()");
    exit(1);
  }
/*
Inter-|   Receive                                                |  Transmit
 face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
    lo:    1956      30    0    0    0     0          0         0     1956      30    0    0    0     0       0          0
  eth0:       0       0    0    0    0     0          0         0   458718       0  781    0    0     0     781          0
  sit0:       0       0    0    0    0     0          0         0        0       0    0    0    0     0       0          0
  eth1:       0       0    0    0    0     0          0         0        0       0    0    0    0     0       0          0
*/
  for(i=0;i<NETMAX;i++) {
    if(fgets(buf,sizeof(buf),fp) == NULL)
      break;
    strip_spaces(buf);
             /* 1   2   3    4   5   6   7   8   9   10   11   12  13  14  15  16 */
    ret = sscanf(&buf[0], "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
    (char *)&p->ifnets[i].if_name,
      &p->ifnets[i].if_ibytes,
      &p->ifnets[i].if_ipackets,
      &p->ifnets[i].if_ierrs,
      &p->ifnets[i].if_idrop,
      &p->ifnets[i].if_ififo,
      &p->ifnets[i].if_iframe,
      &junk,
      &junk,
      &p->ifnets[i].if_obytes,
      &p->ifnets[i].if_opackets,
      &p->ifnets[i].if_oerrs,
      &p->ifnets[i].if_odrop,
      &p->ifnets[i].if_ofifo,
      &p->ifnets[i].if_ocolls,
      &p->ifnets[i].if_ocarrier
      );

    if(ret != 16) {
      fprintf(stderr,"sscanf wanted 16 returned = %d line=%s\n", ret, (char *)buf);
      exit(1);
    }
  }

  fclose(fp);
  fp = NULL;

  data->networks = i;
}

unsigned long long proc_cons(char* protocol, unsigned ip, short port)
{
static FILE *fp = NULL;
char buf[1024]={0};
int ret=0;
struct con_stat con={0};
unsigned long long cons=0;

  sprintf(buf,"/proc/net/%s",protocol);
  if( fp == NULL) {
   if( (fp = fopen(buf,"r")) == NULL) {
      perror("The following error occurred");
      fprintf(stderr,"ERROR: open file %s!\n",buf);
      exit(1);
    }
  }

  rewind(fp);

  if(fgets(buf,sizeof(buf),fp) == NULL)  /* throw away the header lines */
  {
    perror("fgets()");
    exit(1);
  }
/*
sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode     ref pointer        drops
160: 00000000:0045 00000000:0000 07 00000000:00000000 00:00000000 00000000     0        0 18275     2 ffff880033fac000 0
*/
  while(1) {
    if(fgets(buf,sizeof(buf),fp) == NULL)
      break;
    strip_spaces(buf);
                         /* 1   2     3     4  5  6  7  8  9  10 11 12 13 14 15 */
    ret = sscanf(&buf[0], "%llu %x %x %x %x %llx %llx %llx %llx %llx %llx %llx %llx %llx %llx %p",
      &con.sl,
      &con.local_address.ip,
      &con.local_address.port,
      &con.rem_address.ip,
      &con.rem_address.port,
      &con.st,
      &con.tx_queue,
      &con.rx_queue,
      &con.tr,
      &con.tm_when,
      &con.retrnsmt,
      &con.uid,
      &con.timeout,
      &con.inode,
      &con.ref,
      &con.pointer
    );
    if(ret != 16) {
      fprintf(stderr,"sscanf wanted 16 returned = %d line=\n%s\n", ret, (char *)buf);
      exit(1);
    }

    con.rem_address.ip = ntohl(con.rem_address.ip);
    con.local_address.ip = ntohl(con.local_address.ip);

#if 1
    if((con.rem_address.ip == ip) 
        && (con.rem_address.port == port)) {
      cons++;
    }
#else
    if((con.local_address.ip == ip) 
        && (con.local_address.port == port)) {
      cons++;
    }
#endif
  }

  fclose(fp);
  fp = NULL;

  return cons;
}

void GetCpuState(struct cpu_data * data,struct proc * proc){
  int i=0;
  float cpu_usage=0.0;
  unsigned long long cpu_idle=0, cpu_sum=0;
  double elapsed = 0.0;
  struct cpu_stats *p = NULL;
  struct cpu_stats *q = NULL;
  struct cpu_param  *pa = NULL;

/********************************************************
 * cpu statistics 
 ********************************************************/
  if(data->p == NULL) {
    data->p = &data->cpu_stats[0];
    data->q = &data->cpu_stats[1];
  }
  else {
    SWITCHER(data->p,data->q);
  }

  p=data->p;
  q=data->q;
  pa=(struct cpu_param *)&data->cpu_param;

  p->time = doubletime();
  elapsed = p->time - q->time;

  proc_read(proc, P_STAT);
  /* Start with index [1] as [0] contains overall CPU statistics */
  for(i=1; i<proc[P_STAT].lines; i++) {
    if(strncmp("cpu",proc[P_STAT].line[i],3) == 0)
      data->cpus=i;
    else
      break;
  }
  if(data->cpus==0) {
    fprintf(stderr,"ERROR,cpus is %d\n",data->cpus);
    exit(1);
  }

  proc_cpu(data, proc);

  //free memory to avoid memleak
  free(proc[P_STAT].buf);
  proc[P_STAT].buf = NULL;
  proc[P_STAT].size = 0;

  cpu_sum  =(  p->cpu_total.user - q->cpu_total.user
             + p->cpu_total.sys  - q->cpu_total.sys
             + p->cpu_total.wait - q->cpu_total.wait
             + p->cpu_total.idle - q->cpu_total.idle); 
  cpu_idle = p->cpu_total.idle - q->cpu_total.idle; 
  cpu_usage = (cpu_sum-cpu_idle)*100.0/cpu_sum;
  
if (debugl >= 3) {
  printf("\nCpu_usage  Cpu_sum(jiffies),Cpu_idle(jiffies)\n");
  printf("%.1f%%\t%lld\t%lld\n",cpu_usage,cpu_sum,cpu_idle);
}

  pa->total_usage = cpu_usage;

}

void GetFsState(struct dsk_data *data) {
  struct statfs statfs_buffer={0};
  float fs_size=0;
  float fs_bsize=0;
  float fs_free=0;
  int k=0;
  int ret=0;

/********************************************************
 * filesystem inquiry
 ********************************************************/
  data->jfses = jfs_load(data->jfs,LOAD);

if (debugl >= 3) {
  printf("\nfstatfs()\nDevice Usage Free(MB) Sum(MB)\n");
}
  for (k = 0; k < data->jfses; k++) {

    //printf("filesystem %s mounted on %s\n", jfs[k].device, jfs[k].name); 

    if((ret=fstatfs( data->jfs[k].fd, &statfs_buffer)) == -1) {
      perror("fstatfs() The following error occurred");
      exit(1);
    }

    if(statfs_buffer.f_bsize == 0) 
      fs_bsize = 4.0 * 1024.0;
    else
      fs_bsize = statfs_buffer.f_bsize;
    /* convery blocks to MB */
    fs_size = (float)statfs_buffer.f_blocks * fs_bsize/1024.0/1024.0;

    /* fine the best size info available f_bavail is like df reports
       otherwise use f_bsize (this includes inode blocks) */
    if(statfs_buffer.f_bavail == 0) 
      fs_free = (float)statfs_buffer.f_bfree  * fs_bsize/1024.0/1024.0;
    else
      fs_free = (float)statfs_buffer.f_bavail  * fs_bsize/1024.0/1024.0;

    data->jfs[k].bsize = fs_bsize;
    data->jfs[k].size  = fs_size;
    data->jfs[k].free  = fs_free;
    data->jfs[k].usage = fs_size?((fs_size-fs_free)*100/fs_size):0;

if (debugl >= 3) {
    printf("%s\t%.1f%%\t%.1f\t%.1f\n",data->jfs[k].device,data->jfs[k].usage, data->jfs[k].free, data->jfs[k].size);
}
  }

  jfs_load(data->jfs,UNLOAD);

}

void GetDiskState(struct dsk_data *data){
  int i =0, k=0;
  double disk_busy=0;
  double disk_read=0;
  double disk_write=0;
  double disk_xfers=0;
//  double total_disk_read=0;
//  double total_disk_write=0;
//  double total_disk_xfers=0;
  double elapsed = 0.0;

  struct dsk_stats *p = NULL;
  struct dsk_stats *q = NULL;

  char *dk_name=NULL;

/********************************************************
 * disk io statistics 
 ********************************************************/
  GetFsState(data);

  if(data->p == NULL) {
    data->p = &data->dk[0];
    data->q = &data->dk[1];
  }
  else {
    SWITCHER(data->p,data->q);
  }

  p=data->p;
  q=data->q;

  p->time = doubletime();
  elapsed = p->time - q->time;

  proc_disk(data);

  for (k = 0; k < data->jfses; k++) {
    dk_name = strrchr(data->jfs[k].device,'/');

    if(dk_name==NULL) {
      dk_name = data->jfs[k].device;
    }
    else {
      dk_name+=1;
    }

    for(i = 0; i< data->disks; i++) {
      if(!strcmp(dk_name, p->dk[i].dk_name))  break;
    } 

    if(i >= data->disks){
if (debugl >= 2) {
      fprintf(stderr,"WARNING: can't find  %s\tfrom /proc/diskstats\n",dk_name);
}
      continue;
      //exit(1);
    }

  /*
    if(p->dk[i].dk_name[0] == 'h')
      continue;
  */
    disk_busy = DKDELTA(dk_time) / elapsed;
    disk_read = DKDELTA(dk_rkb) / elapsed;
    disk_write = DKDELTA(dk_wkb) / elapsed;
    disk_xfers = DKDELTA(dk_xfers);

//    total_disk_read  +=disk_read;
//    total_disk_write +=disk_write;
//    total_disk_xfers +=disk_xfers;

if (debugl >= 3) {
    printf("\n/proc/diskstats\nDiskName Busy Read(KB/s) Write(KB/s) Xfers(t/s)\n");
    printf("%s %3.1f%% %8.1f %8.1f %8.1f\n",
      p->dk[i].dk_name, 
      disk_busy,
      disk_read,
      disk_write,
      disk_xfers / elapsed);
}

    data->jfs[k].io = disk_busy;

  }
}

void GetMemState(struct mem_data *data,struct proc * proc) {
  double elapsed=0.0;
  unsigned long long mem_total=0;
  unsigned long long mem_free=0;
  float mem_usage=0;
  struct mem_stats *p=NULL;
  struct mem_stats *q=NULL;
  struct mem_param *pa=NULL;
/********************************************************
 * memmory inquiry
 ********************************************************/
  if(data->p == NULL) {
    data->p = &data->mem_stats[0];
    data->q = &data->mem_stats[1];
  }
  else {
    SWITCHER(data->p,data->q);
  }

  p=data->p;
  q=data->q;
  pa=(struct mem_param *)&data->mem_param;

  p->time = doubletime();
  elapsed = p->time - q->time;

  proc_read(proc, P_MEMINFO);

  proc_mem(data->p, proc);

  //free memory to avoid memleak
  free(proc[P_MEMINFO].buf);
  proc[P_MEMINFO].buf = NULL;
  proc[P_MEMINFO].size = 0;

  mem_total = p->mem_stat.memtotal;
  mem_free = p->mem_stat.memfree + p->mem_stat.buffers + p->mem_stat.cached;
  mem_usage = (mem_total-mem_free)*100.0/mem_total;

if (debugl >= 3) {
  printf("\nMemory_usage Sum(KB) Free(KB) (free(KB) + buffers(KB) + cached(KB))\n");
  printf("%.1f%%\t%lld\t%lld(%lld + %lld +%lld)\n",mem_usage, mem_total, mem_free, p->mem_stat.memfree, p->mem_stat.buffers, p->mem_stat.cached);
}
  
  pa->usage =  mem_usage;
}

void GetNetworkState(struct net_data *data) {
  int i=0,j=0;
  double elapsed=0.0;
  struct net_stats *p = NULL;
  struct net_stats *q = NULL;
  struct net_param *pa=NULL;

/********************************************************
 * net statistics 
 ********************************************************/
  socket_net(data);

  if(data->p == NULL) {
    data->p = &data->net_stats[0];
    data->q = &data->net_stats[1];
  }
  else {
    SWITCHER(data->p,data->q);
  }

  p=data->p;
  q=data->q;
  pa=(struct net_param *)&data->net_param;

  p->time = doubletime();
  elapsed = p->time - q->time;

  proc_net(data);

if (debugl >= 3) {
  printf("\n/proc/net/dev\nIF_Name Recv=Kb/s Trans=Kb/s packin(p/s) packout(p/s) inpacksize(B) outpacksize(B) Peak->Recv(Kb/s) Peak->Trans(Kb/s)\n");
}
  for(j=0; j<data->nets; j++) {
    for(i=0; i<data->networks; i++) {
      if(!strcmp(pa[j].name, p->ifnets[i].if_name)) {
        break;
      }
    }

    if(i>=data->networks) {
      fprintf(stderr,"ERROR: can't find net if %s from /proc/net/dev\n",pa[j].name);
      exit(1);
    }

    pa[j].speed_ib = IFDELTA(if_ibytes);
    pa[j].speed_ob = IFDELTA(if_obytes);
    pa[j].speed_ip = IFDELTA(if_ipackets);
    pa[j].speed_op = IFDELTA(if_opackets);
    pa[j].size_i   = IFDELTA_ZERO(if_ibytes, if_ipackets);
    pa[j].size_o   = IFDELTA_ZERO(if_obytes, if_opackets);

    pa[j].usage = (/*MAX(pa[i].speed_ib,*/ pa[i].speed_ob/*)*/*8*100)/(pa[i].bandwidth*1000*1000);

    if (pa[j].peak_i < pa[j].speed_ib) {
      pa[j].peak_i = pa[j].speed_ib;
    }

    if (pa[j].peak_o < pa[j].speed_ob) {
      pa[j].peak_o = pa[j].speed_ob;
    }

if (debugl >= 3) {
        printf("%s\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\n",
        pa[j].name,
        pa[j].speed_ib*8/1024,
        pa[j].speed_ob*8/1024,
        pa[j].speed_ip,
        pa[j].speed_op,
        pa[j].size_i,
        pa[j].size_o,
        pa[j].peak_i*8/1024,
        pa[j].peak_o*8/1024
      );
}

  //    if(!strcmp(pa[i].ip, ip)) {
  //      break;
  //    }
  //  
  //
  //  if(i>=data->nets) {
  //    fprintf(stderr,"ERROR: can't find ip %s from net_param\n",ip);
  //    exit(1);
  //  }

  }

if (debugl >= 3) {
  printf("\nioctl():\nI/F_Name Flags Bandwidth(Mb) Duplex Ip Ipstate Usage\n");
}

  for(j=0; j<data->nets; j++) {

if (debugl >= 3) {
    printf("%s\t%hd\t%ld\t%d\t%s\t%7.3f%%\n",
        pa[j].name,
        pa[j].flags,
        pa[j].bandwidth,
        pa[j].duplex,
        pa[j].ip,
        pa[j].usage
    );
}
  }

}

unsigned ConvertIpC2I(char * ip_char) {
  unsigned int ip_int=0;
  char str[IP_LEN]={0};
  char delims[] = ".";
  char *result = NULL;
  int i=0;

  strcpy(str, ip_char);
  i = 4;
  result = strtok( str, delims );
  while( result != NULL ) {
    //printf( "result is \"%s\"", result );
    ip_int |= strtol(result,NULL,10) << ((--i)*8);
    result = strtok( NULL, delims );
  }

  if(i!=0) {
    fprintf(stderr,"ERROR: ip %s is illegal\n",ip_char);
    exit(1);
  }

  return ip_int;

}

unsigned long long GetCurrentConn(char* ip_char, short port){
  unsigned long long cons=0;
  unsigned int ip_int=0;
/********************************************************
 * connections inquiry
 ********************************************************/
  ip_int = ConvertIpC2I(ip_char);

  cons = proc_cons("tcp",ip_int,port);

if (debugl >= 3) {
  printf("\nNumber of conncetions to (ip: %s(%#x) port: %d(%#x)) is %llu\n",ip_char, ip_int, port,port, cons);
}

  return cons;
}