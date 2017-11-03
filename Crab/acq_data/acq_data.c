#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <omp.h>
#include "acq_udp.h"

#define  GET_SEQ_NUM(){			\
    memcpy(&pos,sockpar->ubuf,UDP_HDR);	\
  }/* Get packet header*/

long long GET_NEXT_PKT(SockPar *sockpar);
void init_sock_stat(SockStat *stat);
void report_socket_stat(SockStat *stat);
int init_socket(SockPar *sockpar);
int acquire_socket_data(SockPar *sockpar);
int transfer_socket_data(SockPar *sockpar);

long long GET_NEXT_PKT(SockPar *sockpar){
  unsigned long long pos=0ULL,seq=0ULL;
  while(recvfrom(sockpar->fd,sockpar->ubuf,UDP_PAYLOAD,0,NULL,NULL)!=UDP_PAYLOAD){
    if(errno != EAGAIN){
      perror("acquire_socket_data");
      return 1;
    }
  }
  GET_SEQ_NUM();
//fprintf(stderr, "%llu : \n", pos);
pos = (unsigned long long)pos/512;
  if (pos%8 >=3 && pos%8<=5){
    seq = 3*((unsigned long long)pos/8) + pos%8 -3;
    //fprintf(stderr, "%llu : %llu\n", pos, seq);
    return seq;
  }
  else GET_NEXT_PKT(sockpar);
}/*Get UDP_PAYLOAD size data packet from the socket*/

static int DoFinish=0;

void init_sock_stat(SockStat *stat){ 

  stat->total         = 0;
  stat->got           = 0;
  stat->lost          = 0;
  stat->bad           = 0;
  stat->rate          = 0;
  stat->dtotal        = 0;
  stat->dgot          = 0;
  stat->dlost         = 0;
  stat->dbad          = 0;
  stat->drate         = 0;
  stat->start.tv_sec  = 0;
  stat->start.tv_nsec = 0;
  stat->dstart.tv_sec = 0;
  stat->dstart.tv_nsec= 0;
  stat->log_rate      = NACC; // should be a multiple of NACC

  return;
}

void report_socket_stat(SockStat *stat){
  struct timespec curr_time;
  double          dt,ddt;
  time_t          now=time(0);
  char            time[100];
  
  clock_gettime(CLOCK_MONOTONIC,&curr_time);
  dt=(curr_time.tv_sec + curr_time.tv_nsec/1.0e9)-(stat->start.tv_sec 
				   + stat->start.tv_nsec/1.0e9);
  ddt=(curr_time.tv_sec + curr_time.tv_nsec/1.0e9)-(stat->dstart.tv_sec 
				   + stat->dstart.tv_nsec/1.0e9);
  strftime(time,100,"%Y-%m-%d_%T", localtime(&now));
  
  /* report the statistics */
  
  fprintf(stderr,"Time: %s\n",time);
  fprintf(stderr,"Cumulative  Stats: Total/Got/Lost/Bad  %ld/%ld/%ld/%ld\n",
	  stat->total+stat->lost+stat->bad,stat->got,stat->lost,stat->bad);
  fprintf(stderr,"Differential Stats: Total/Got/Lost/Bad  %ld/%ld/%ld/%ld\n",
	  stat->dtotal+stat->dlost+stat->dbad,stat->dgot,stat->dlost,
	  stat->dbad);
  fprintf(stderr,"Cummlative/Differental Rate:  %10.2f/%-10.2f (Mbytes/sec)\n",
	  stat->got*UDP_PAYLOAD/(1024.0*1024.0*dt),
	  stat->dgot*UDP_PAYLOAD/(1024.0*1024.0*ddt));
  fprintf(stderr,"Cummlative/Differental Loss:     %10.2f/%-10.2f\n",
	  stat->lost/(1.0*stat->total),stat->dlost/(1.0*stat->dtotal));
 
  /* reset the differential statistics counters */
  stat->dstart = curr_time;
  stat->dtotal = 0;
  stat->dgot   = 0;
  stat->dlost  = 0;
  stat->dbad   = 0;

  return;
}

int init_socket(SockPar *sockpar){
  int i,j;

  /* set the default socket parameters */
  sockpar->fd                   = -1;
  sockpar->addr.sin_family      = PF_INET;  
  sockpar->addr.sin_addr.s_addr = inet_addr(UDP_IP);
  sockpar->addr.sin_port        = htons(UDP_PORT); 

  /* set the accum buffer parameters */
  for(i=0;i<NSOCKBUF;i++){
    SockBuf *sbuf=sockpar->sbuf+i;
    if((sbuf->data=malloc(ACC_BUFSIZE))==NULL){
      fprintf(stderr,"init_socket() Malloc Failure\n");
      return 1;
    }
    if((sbuf->flag=malloc(ACC_FLGSIZE))==NULL){
      fprintf(stderr,"init_socket() Malloc Failure\n");
      return 1;
    }
    for(j=0;j<NACC;j++)sbuf->flag[j]=1; // all data pre-flagged
    sbuf->start =  0;
    sbuf->stop  =  0;
    sbuf->count =  0;
    sbuf->idx   =  i;
  }   
  
  /* initialise pointers to curr, next and copy buffers */
  sockpar->curr     = &sockpar->sbuf[0];
  sockpar->next     = &sockpar->sbuf[1];
  sockpar->copy     = NULL;

  //fprintf(stderr,"init_socket(): Value of preflagged flag is:%u\n",*(sockpar->curr->flag));
  
  /* threshold filling fraction of the "next" buffer. If more than this
     fraction of the "next" buffer is filled, stop waiting for packets from 
     the "current" buffer */
  sockpar->switch_thresh  = 0.5;

  init_sock_stat(&sockpar->stat);
  
  return 1;
}

int acquire_socket_data(SockPar *sockpar){

  SockStat      *stat=&sockpar->stat;
  unsigned      i,j;
  unsigned long long seq,seq_tmp,last_seq;
  int           ngood,ntry,idx;
  unsigned long offset=0;

  while(!DoFinish){
    seq = GET_NEXT_PKT(sockpar);
      if(sockpar->curr->start==0){
	/* need to find a place to  start. Wait till we have 3 successive 
	   spectra (8 packets) with consecutive packet numbers */
	ngood    = 0;
	last_seq = seq;
	ntry     = 0;
	while(ngood<3 || seq%3!=0){
	  seq = GET_NEXT_PKT(sockpar);
          //fprintf(stderr,"%llu ",seq);
	  if(seq-last_seq == 1) ngood++;
	  else ngood=0;
	  last_seq=seq; ntry++;
	  if(ntry>1024){
	    fprintf(stderr,"acq: Can't find a good place to start!\n");
	    return -1;
	  }
	}
      fprintf(stderr,"acq: Starting with PktNo %llu\n",seq);
      sockpar->curr->start = seq;
      sockpar->curr->stop  = seq+NACC; // first packet of next buffer
      sockpar->next->start = sockpar->curr->stop; 
      sockpar->next->stop  = sockpar->next->start+NACC;
      clock_gettime(CLOCK_MONOTONIC,&stat->start);
      stat->dstart=stat->start;
    }
    
    /* copy packet data to the correct location, if possible */
    if(seq >= sockpar->curr->start && seq < sockpar->curr->stop){
      /* packet belongs to current buffer */
      offset=(seq-sockpar->curr->start);
      if(!sockpar->curr->flag[offset]){
	//fprintf(stderr,"curr: bad pkt");
	/* duplicate packet! */
	stat->bad++;stat->dbad++;
      }else{
	sockpar->curr->flag[offset]=0;//seq%8;
	//fprintf(stderr,"curr: flag= %d\n",sockpar->curr->flag[offset]);
	offset=offset*UDP_DATA;
	memcpy(sockpar->curr->data+offset,sockpar->ubuf+UDP_HDR,UDP_DATA);
	sockpar->curr->count++;
	stat->got++; stat->dgot++;
      }
    }else{
      if(seq >= sockpar->next->start && seq < sockpar->next->stop){
	/* packet belongs to next buffer */
	offset=(seq-sockpar->next->start);
	if(!sockpar->next->flag[offset]){
	  /* duplicate packet */
	  //fprintf(stderr,"next: bad pkt");
	  stat->bad++;stat->dbad++;
	}else{
	  sockpar->next->flag[offset]=0;
	  //fprintf(stderr,"next: flag= %d",sockpar->next->flag[offset]);
	  offset=offset*UDP_DATA;
	  memcpy(sockpar->next->data+offset,sockpar->ubuf+UDP_HDR,UDP_DATA);
	  sockpar->next->count++;
	  stat->got++;stat->dgot++;
	}
      }else{
	/* packet is badly out of time, drop */
	//fprintf(stderr,"out of time");
	stat->bad++;stat->dbad++;
      }
    }
    
    /* Switch buffers if the current buffer is full, or if more than
       the switch_thresh of the next buffer is full */
    if((sockpar->curr->count == NACC) || 
       (sockpar->next->count > sockpar->switch_thresh*NACC)){
      //fprintf(stderr,"acquire_socket_data(): Switch copy to idx:%d\n",sockpar->curr->idx);
      /* update number of lost packets */
      stat->total         += NACC;
      stat->dtotal        += NACC;
      stat->lost          += (NACC-sockpar->curr->count);
      stat->dlost         += (NACC-sockpar->curr->count);
      sockpar->copy        = sockpar->curr;
      sockpar->curr        = sockpar->next;
      /* update the "next" buffer pointer and preflag all data */
      idx                  = (sockpar->next->idx+1)%NSOCKBUF;
      sockpar->next        = sockpar->sbuf+idx;
      sockpar->next->count = 0;
      sockpar->next->start = sockpar->curr->stop;
      sockpar->next->stop  = sockpar->next->start+NACC;
      for(i=0;i<NACC;i++) sockpar->next->flag[i]=1;
    }
    

    /* Start over if you get a lot of bad packets */
    if(stat->dbad > .75*NACC){
      fprintf(stderr,"\n\nGot a lot of bad packets. Resetting.\n\n");
      //sockpar->curr->start=0;
      while (seq%3 != 0){
      	seq = GET_NEXT_PKT(sockpar);
      }
      sockpar->curr->start = seq;
      sockpar->curr->stop  = seq+NACC; // first packet of next buffer
      sockpar->next->start = sockpar->curr->stop; 
      sockpar->next->stop  = sockpar->next->start+NACC;
      for(i=0;i<NACC;i++) sockpar->next->flag[i]=1;

      for(i=0;i<NACC;i++) sockpar->curr->flag[i]=1;

      clock_gettime(CLOCK_MONOTONIC,&stat->start);
      stat->dstart=stat->start;
      stat->bad=0;
      stat->got=0;
    }

    /* report statististics periodically */
    if((stat->dgot + stat->dbad) == stat->log_rate){
      report_socket_stat(stat);
      //fprintf(stderr, "seq: %llu\n", seq);
    }

  }//DoFinish
    
    /* free buffers, allocation was done in init_socket() */
    for(i=0;i<NSOCKBUF;i++){
      free(sockpar->sbuf[i].data);
      free(sockpar->sbuf[i].flag);
    }

    return 1;
}

int transfer_socket_data(SockPar *sockpar){

  unsigned int     max_pkt = (FILESIZE*1024*1024)/ACC_BUFSIZE;
  unsigned int     npkt=0,i,idx0,idx,sleep_time;
  char             filename[50];
  char             micro[15];
  SockBuf          *idxc;
  FILE             *fp;
  struct tm        *now;
  time_t           rawtime;
  struct timespec  start;
  idx0              = NSOCKBUF-1;
  sleep_time        = 0.01; // micro seconds

  fprintf(stderr,"Listening on %s:%d \n",
	  inet_ntoa(sockpar->addr.sin_addr),ntohs(sockpar->addr.sin_port));

  /* Include time and date info in the data file*/
  time(&rawtime);
  now = localtime(&rawtime);
  clock_gettime(CLOCK_MONOTONIC_RAW, &start); 
  uint32_t us = start.tv_nsec;
  sprintf(micro, "%u", us);
  strftime(filename,sizeof(filename), "/data0/data_nov-03-2017/data_%Y-%m-%d_%H-%M-%S_", now);	
  strcat(filename,micro);
  fprintf(stderr, "Filename  %s\n", filename);
  fp=fopen(filename,"a+");
  
  while(!DoFinish){
    if(sockpar->copy != NULL && (idx=sockpar->copy->idx) != idx0){
      //fprintf(stderr,"transfer_socket_data: copy idx :%d\n",idx);
      /* got fresh data*/
      if(idx != (idx0+1)%NSOCKBUF) // missed at least one buf!
	fprintf(stderr,"\n\n\ntransfer_socket_data(): missed copying a buffer!\n\n\n\n");
      
      idx0=idx;
      idxc = sockpar->copy;
      
      if (npkt==max_pkt){
	fclose(fp);
	npkt=0;
	time(&rawtime);
	now = localtime(&rawtime);
        clock_gettime(CLOCK_MONOTONIC_RAW, &start); 
        uint32_t us = start.tv_nsec;
        sprintf(micro, "%u", us);
        strftime(filename,sizeof(filename), "/data0/data_nov-03-2017/data_%Y-%m-%d_%H-%M-%S_", now);	
        strcat(filename,micro);
	fp=fopen(filename,"a+");
      }
      fwrite(idxc->data, UDP_DATA, NACC, fp);
      fwrite(idxc->flag, sizeof(unsigned char), NACC, fp);
      npkt++;
    }else{ /* data not available yet, check again*/
      //usleep(sleep_time);
    }
  }
  return 0;
}

int main(int argc, char **argv){
  SockPar          sockpar;
  SockStat        *stat=&sockpar.stat;
  int              tid,arg;
  unsigned short   port;
  int              sockbufsize=1024*1024*1024; // 512 MB socket buffer

  init_socket(&sockpar);

  /* check if the user want's to override the defaults */
  while((arg=getopt(argc,argv,"i:p:")) !=-1){
    switch(arg){
    case 'i':
      fprintf(stderr,"inet_addr= %s\n",optarg);
      if(inet_aton(optarg,&sockpar.addr.sin_addr)==0){
	fprintf(stderr,"Illegal IP address %s\n",optarg);
	return 1;
      }
      break;
    case 'p':
      port=(unsigned short)atoi(optarg);
      sockpar.addr.sin_port = htons(port); 
      break;
    default:
      fprintf(stderr,"Usage: acq [-i ip][-p port]\n");
      return 1;
    }
  }

  /* open the socket and bind to the specified address */
  if((sockpar.fd=socket(AF_INET,SOCK_DGRAM|SOCK_NONBLOCK,IPPROTO_UDP))<0){
    perror("acq: Can't open socket");
    return 1;
  }
  if(bind(sockpar.fd,(struct sockaddr*)&sockpar.addr,
	  (socklen_t)sizeof(struct sockaddr))){
    perror("acq: Can't bind socket");
    return 1;
  }


  /* set the socket buffer size to some large value */
  if(setsockopt(sockpar.fd, SOL_SOCKET, SO_RCVBUF, &sockbufsize, sizeof(int))){
    perror("acq: Can't set SockBufSize");
    return 1;
  }

#pragma omp parallel num_threads(2) private (tid) shared(sockpar)
  { tid  = omp_get_thread_num(); 
    if(tid==0){
      acquire_socket_data(&sockpar);  // copy data from socket 
    }else{
      transfer_socket_data(&sockpar); //transfer socket data
    }
  }

  return 0;
}
