//============================================================================
// Author      : Soheil Abbasloo (abbasloo@cs.toronto.edu)
// Version     : V3.0
//============================================================================

/*
  MIT License
  Copyright (c) 2022 Soheil Abbasloo

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#define EVALUATION 1
#define DS_VERSION 820
#define STANDALONE 1    //Comment this line if you're using Pantheon


#define USER "soheil"

#include <cstdlib>
#include <sys/select.h>
#include "define.h"

// Wait "WAIT_FOR_ACTION_MAX_ms" for getting action from RL agent, 
// beyond that, simply assume no action is received and go for another state report to RL agent.
#define WAIT_FOR_ACTION_MAX_ms 200 

//#define CHANGE_TARGET 1
#define FLOORING 0
#define ROUNDING 1
#define CEILING  2
#define NONE     3
#define ROUNT_TYPE CEILING

#define TURN_ON_SAFETY 0
#define SAFETY_MARGIN 3 //10

#define ACCUMULATE_CWND 1

#define SHORT_WIN   10
#define MID_WIN     200
#define LONG_WIN    1000

#define ESTIMATE 0
#define ACCURACY 10000.0 //1000.0 //100.0 //100.0 //10000.0    //Final Cwnd precision: 1/ACCURACY

#define BDP_SRTT 1
#define BDP_MINRTT 2
#define UPPERBOUND_TYPE BDP_MINRTT

//#define SLOW_START_MANUAL
//Followings only will be used if SLOW_START_MANUAL is defined.
#define SLOW_START_STEP_WAIT_TIME 1//n x rtt
#define SLOW_START_INIT_STATE false  //false
#define SLOW_START_TIME_MS 1000 //ms
#define SLOW_START_TRANSITION_STEPS 1 //10
#define SLOW_START_COEF 2.0 //2.0 //1.15 
#define SLOW_START_CUTOFF 1000 //0x10000000  
#define SLOW_START_MAX_BW_IDLE_CUTOFF 1 //3 //0x10000000  

#define WIN_SIZE 500
//#define WIN_SIZE 10000

#define PACE_ENABLE 1
#define PACE_TYPE PACE_WITH_SRTT
#define PACE_COEF 100

#define MAX_CWND 20000
#define CWND_INIT 10
#define MIN_CWND 4 //CWND_INIT //1
#define STEPS_UPDATE_MAX_CWND_EFF 10
#define MAX_CWND_EFF_INIT 100
#define BW_NORM_FACTOR 100     //100 Mbps will be used to normalize throughput signal

FILE *testing_;
enum fsm{
    state_DRL=1,
    state_Fair=2
};
fsm cc_state=state_DRL;
int save=1;
u32 iterations=0;
u64 send_begun;
int main(int argc, char **argv)
{
    DBGPRINT(DBGSERVER,4,"Main\n");
    if(argc!=20)
	{
        DBGERROR("argc:%d\n",argc);
        for(int i=0;i<argc;i++)
        	DBGERROR("argv[%d]:%s\n",i,argv[i]);
		usage();
		return 0;
	}

    srand(raw_timestamp());

	signal(SIGSEGV, handler);   // install our handler
	signal(SIGTERM, handler);   // install our handler
	signal(SIGABRT, handler);   // install our handler
	signal(SIGFPE, handler);   // install our handler
    signal(SIGKILL,handler);   // install our handler
    int flow_num;

	flow_num=FLOW_NUM;
	client_port=atoi(argv[1]);
    path=argv[2];
    save=atoi(argv[3]);
    flows_num=atoi(argv[4]);
    qsize=100;//;atoi(argv[8]);
    report_period=10; //atoi(argv[5]);
	codel=2;//atoi(argv[10]);
   	env_bw=atoi(argv[5]);
    first_time=atoi(argv[6]);
    scheme=argv[7];//"pure";
    actor_id=atoi(argv[8]);

    downlink=argv[9];
    uplink=argv[10];
    delay_ms=atoi(argv[11]);
    log_file=argv[12];
    duration=atoi(argv[13]);
    mm_loss_rate=atof(argv[14]); 
    qsize=atoi(argv[15]);
    duration_steps=atoi(argv[16]);
    basetime_path=argv[17];
    bw2=atoi(argv[18]);
    trace_period=atoi(argv[19]);

	DBGPRINT(DBGSERVER,5,"********************************************************\n starting the server ...\n");
    start_server(flow_num, client_port);
#ifdef EVALUATION
    shmdt(shared_memory);
    shmctl(shmid, IPC_RMID, NULL);
    shmdt(shared_memory_rl);
    shmctl(shmid_rl, IPC_RMID, NULL);
#endif
    return 0;
}

void usage()
{
	DBGERROR("./server [port] [path to run.py] [Save] [Number of Flows] [Report Period: 10 msec--fixed for now :)] [First Time: 1=yes(learn), 0=no(continue learning), 2=evaluate] [scheme: pure] [actor id=0, 1, ...] [downlink] [uplink] [one-way delay] [log_file] [duration] [loss rate] [qsize] [duration_steps] [folder containing basetimestamp]\n");
}

void start_server(int flow_num, int client_port)
{
    cFlow *flows;
    int num_lines=0;
	//FILE* filep;
	sInfo *info;
	info = new sInfo;
	flows = new cFlow[flow_num];
	if(flows==NULL)
	{
		DBGERROR("flow generation failed\n");
		return;
	}

	//threads
	pthread_t data_thread;
	pthread_t cnt_thread;
	pthread_t timer_thread;

	//Server address
	struct sockaddr_in server_addr[FLOW_NUM];
	//Client address
	struct sockaddr_in client_addr[FLOW_NUM];
	//Controller address
    for(int i=0;i<FLOW_NUM;i++)
    {
        memset(&server_addr[i],0,sizeof(server_addr[i]));
        //IP protocol
        server_addr[i].sin_family=AF_INET;
        //Listen on "0.0.0.0" (Any IP address of this host)
        server_addr[i].sin_addr.s_addr=INADDR_ANY;
        //Specify port number
        server_addr[i].sin_port=htons(client_port+i);

        //Init socket
        if((sock[i]=socket(PF_INET,SOCK_STREAM,0))<0)
        {
            DBGERROR("sockopt: %s\n",strerror(errno));
            return;
        }

        int reuse = 1;
        if (setsockopt(sock[i], SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
            perror("setsockopt(SO_REUSEADDR) failed");
        //Bind socket on IP:Port
        if(bind(sock[i],(struct sockaddr *)&server_addr[i],sizeof(struct sockaddr))<0)
        {
            DBGERROR("[Actor %d] bind error srv_ctr_ip: 000000: %s\n",actor_id,strerror(errno));
            close(sock[i]);
            return;
        }
        if (scheme) 
        {
            if (setsockopt(sock[i], IPPROTO_TCP, TCP_CONGESTION, scheme, strlen(scheme)) < 0) 
            {
           		DBGMARK(DBGSERVER,5,"Scheme (%s) Failed \n",scheme);
                DBGERROR("TCP congestion doesn't exist: %s\n",strerror(errno));
                return;
            } 
        }

    }

    
    char container_cmd[500];
    char tmp_cmd[500];
    char pre_cmd[500];
    sprintf(tmp_cmd," ");
    sprintf(pre_cmd," ");
    char cmd[1000];
    char final_cmd[1000];
    
#ifdef EVALUATION
#ifdef STANDALONE
    for(int i=1;i<flows_num;i++)
    {      
        sprintf(tmp_cmd,"%s sudo -u `echo $USER` iperf3 -c $MAHIMAHI_BASE -p %d -R -i 0 -t %d &",tmp_cmd,client_port+i,duration);
        sprintf(pre_cmd,"%s sudo -u `echo $USER` iperf3 -s -p %d -i 0 &",pre_cmd,client_port+i);
    }

    sprintf(container_cmd,"%s sudo -u `echo $USER` %s/client $MAHIMAHI_BASE 1 %d",tmp_cmd,path,client_port);  
    if(save)
    {
        sprintf(cmd, "%s sudo -u `echo $USER` mm-delay %d mm-loss downlink %.7f mm-loss uplink %.7f mm-link %s/../../traces/%s %s/../../traces/%s --downlink-log=%s/../log/down-%s --uplink-queue=droptail --uplink-queue-args=\"packets=%d\" --downlink-queue=droptail --downlink-queue-args=\"packets=%d\" -- sh -c \'%s\' &",pre_cmd,delay_ms,mm_loss_rate,mm_loss_rate,path,uplink,path,downlink,path,log_file,qsize,qsize,container_cmd);
    }
    else
    {
       sprintf(cmd, "%s sudo -u `echo $USER` mm-delay %d mm-loss downlink %.7f mm-loss uplink %.7f mm-link %s/../../traces/%s %s/../../traces/%s --uplink-queue=droptail --uplink-queue-args=\"packets=%d\" --downlink-queue=droptail --downlink-queue-args=\"packets=%d\" -- sh -c \'%s\' &",pre_cmd,delay_ms,mm_loss_rate,mm_loss_rate,path,uplink,path,downlink,qsize,qsize,container_cmd);
    }
    sprintf(final_cmd,"%s",cmd);
#endif
    info->trace=trace;
    info->num_lines=num_lines;
    /**
     *Setup Shared Memory
     */ 
    key=(key_t) (actor_id*10000+rand()%10000+client_port);
    key_rl=(key_t) (actor_id*10000+rand()%10000+client_port);
    // Setup shared memory, 11 is the size
    
    if ((shmid = shmget(key, shmem_size, IPC_CREAT | 0666)) < 0)
    {
        printf("Error getting shared memory id");
        return;
    }
    // Attached shared memory
    if ((shared_memory = (char*)shmat(shmid, NULL, 0)) == (char *) -1)
    {
        printf("Error attaching shared memory id");
        return;
    }
    // Setup shared memory, 11 is the size
    if ((shmid_rl = shmget(key_rl, shmem_size, IPC_CREAT | 0666)) < 0)
    {
        printf("Error getting shared memory id");
        return;
    }
    // Attached shared memory
    if ((shared_memory_rl = (char*)shmat(shmid_rl, NULL, 0)) == (char *) -1)
    {
        printf("Error attaching shared memory id");
        return;
    } 

    if (first_time==0)
    {
    DBGERROR("Starting RL in Evalution Mode(0) ...\n%s",cmd);
    sprintf(cmd,"/home/`echo $USER`/venvpy36/bin/python %s/tcpactor.py --base_path=%s --mode=0 --mem_r=%d --mem_w=%d --id=%d --flows=%d --bw=%d&",path,path,(int)key,(int)key_rl,actor_id,flows_num,env_bw);
    }
    else if (first_time==1)
    {
        DBGERROR("Starting RL in Training Mode(1) ...\n%s",cmd);
        sprintf(cmd,"/home/`echo $USER`/venvpy36/bin/python %s/tcpactor.py --base_path=%s --mode=1 --mem_r=%d --mem_w=%d --id=%d --flows=%d --bw=%d&",path,path,(int)key,(int)key_rl,actor_id,flows_num,env_bw);
    }
    else if (first_time==2)
    {
        DBGERROR("Starting RL in Evaluation during Training Mode(1) ...\n%s",cmd);
        sprintf(cmd,"/home/`echo $USER`/venvpy36/bin/python %s/tcpactor.py --base_path=%s --mode=2 --mem_r=%d --mem_w=%d --id=%d --flows=%d --bw=%d&",path,path,(int)key,(int)key_rl,actor_id,flows_num,env_bw);
    }

    system(cmd);
    //Wait to get OK signal (alpha=OK_SIGNAL)
    DBGERROR("(Actor %d) Waiting for RL Module ...\n",actor_id);
#endif
    
    bool got_ready_signal_from_rl=false;
    int signal;
    char *num;
    char*alpha;
    char *save_ptr;
    int signal_check_counter=0;

#ifdef EVALUATION
    while(!got_ready_signal_from_rl)
    {
        //Get alpha from RL-Module
        signal_check_counter++;
        num=strtok_r(shared_memory_rl," ",&save_ptr);
        alpha=strtok_r(NULL," ",&save_ptr);
        if(num!=NULL && alpha!=NULL)
        {
           signal=atoi(alpha);      
           if(signal==OK_SIGNAL)
           {
              got_ready_signal_from_rl=true;
           }
           else{
               usleep(10000);
           }
        }
        else{
           usleep(10000);
        }
        if (signal_check_counter>6000)
        {
            DBGERROR("After 1 minute, no response (OK_Signal) from the Actor %d is received! We are going down down down ...\n",actor_id);
            return;
        }   
    }
    
    DBGERROR("(Actor %d) RL Module is Ready. Let's Start ...\n\n",actor_id);    
    usleep(actor_id*10000+10000);
    //Now its time to start the server-client app ...
    initial_timestamp();
    system(final_cmd);
#endif
	//Give Mahimahi enough time to write the BaseTimestamp to the file! ;)
    //usleep(500000);
    FILE *filep;
    char line[4096];
    char basetime_file_name[1000];
    sprintf(basetime_file_name,"%s",basetime_path);
    int read_cnt=0;
    bool file_read=false;
    while((read_cnt<40)&&(!file_read))
    {
        if((filep=fopen(basetime_file_name,"r"))!=NULL)
        {
            file_read = true;
            fgets(line, sizeof(line), filep);
            sscanf(line, "%" SCNu64, &start_timestamp);
            fclose(filep);
        }
        read_cnt++;
        usleep(100000);
    }
    if (!file_read)
    {
        DBGERROR("Tried for 40x to read <basetime> file! Seems it doesn't exist! You haven't patched Mahimahi dude?!\n");
        start_timestamp = 0;
        return;
    }
    //Start listen
    int maxfdp=-1;
    fd_set rset; 
    FD_ZERO(&rset);
    //The maximum number of concurrent connections is 1
	for(int i=0;i<FLOW_NUM;i++)
    {
        listen(sock[i],10);
        //To be used in select() function
        FD_SET(sock[i], &rset); 
        if(sock[i]>maxfdp)
            maxfdp=sock[i];
    }

    //Timeout {2min}, if something goes wrong! (Maybe  mahimahi error ...!)
    maxfdp=maxfdp+1;
    struct timeval timeout;
    timeout.tv_sec  = 120; //2 * 60;
    timeout.tv_usec = 0;
    int rc = select(maxfdp, &rset, NULL, NULL, &timeout);
    /**********************************************************/
    /* Check to see if the select call failed.                */
    /**********************************************************/
    if (rc < 0)
    {
        DBGERROR("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-==-=-=-=-=-=-=- select() failed =-=-=-=-=-=--=-=-=-=-=\n");
        return;
    }
    /**********************************************************/
    /* Check to see if the X minute time out expired.         */
    /**********************************************************/
    if (rc == 0)
    {
        DBGERROR("=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-==-=-=-=-=-=-=- select() Timeout! =-=-=-=-=-=--=-=-=-=-=\n");
        return;
    }

	int sin_size=sizeof(struct sockaddr_in);
	while(flow_index<flow_num)
	{
        if (FD_ISSET(sock[flow_index], &rset)) 
        {
            int value=accept(sock[flow_index],(struct sockaddr *)&client_addr[flow_index],(socklen_t*)&sin_size);
            if(value<0)
            {
                perror("accept error\n");
                DBGERROR("sockopt: %s\n",strerror(errno));
                DBGERROR("sock::%d, index:%d\n",sock[flow_index],flow_index);
                close(sock[flow_index]);
                return;
            }
            sock_for_cnt[flow_index]=value;
            flows[flow_index].flowinfo.sock=value;
            flows[flow_index].dst_addr=client_addr[flow_index];
            if(pthread_create(&data_thread, NULL , DataThread, (void*)&flows[flow_index]) < 0)
            {
                DBGERROR("could not create thread\n");
                close(sock[flow_index]);
                return;
            }
                      
                if (flow_index==0)
                {
                    
                    if(pthread_create(&cnt_thread, NULL , CntThread, (void*)info) < 0)
                    {
                        DBGERROR("could not create control thread\n");
                        close(sock[flow_index]);
                        return;
                    }
                    
                    if(pthread_create(&timer_thread, NULL , TimerThread, (void*)info) < 0)
                    {
                        DBGERROR("could not create timer thread\n");
                        close(sock[flow_index]);
                        return;
                    } 
                }
                
            DBGERROR("(Actor %d) Server is Connected to the client...\n",actor_id);
            flow_index++;
        }
    }
    //fclose(testing_);
    pthread_join(data_thread, NULL);
}

void* TimerThread(void* information)
{
    uint64_t start=timestamp();
    unsigned int elapsed; 
    if ((duration!=0))
    {
        while(send_traffic)
        {
            sleep(1);
            elapsed=(unsigned int)((timestamp()-start)/1000000);      //unit s
            if (elapsed>duration)    
            {
                send_traffic=false;
            }
        }
    }

    return((void *)0);
}
void* CntThread(void* information)
{
/*    struct sched_param param;
    param.__sched_priority=sched_get_priority_max(SCHED_RR);
    int policy=SCHED_RR;
    int s = pthread_setschedparam(pthread_self(), policy, &param);
    if (s!=0)
    {
        DBGERROR("Cannot set priority (%d) for the Main: %s\n",param.__sched_priority,strerror(errno));
    }

    s = pthread_getschedparam(pthread_self(),&policy,&param);
    if (s!=0)
    {
        DBGERROR("Cannot get priority for the Data thread: %s\n",strerror(errno));
    }
    */
	int ret1;
    double min_rtt_=0.0;
    double pacing_rate=0.0;
    double lost_bytes=0.0;
    double lost_rate=0.0;
    double srtt_ms=0.0;
    double snd_ssthresh=0.0;
    double packets_out=0.0;
    double retrans_out=0.0;
    double max_packets_out=0.0;

	int reuse = 1;
    int pre_id=9230;
    int pre_id_tmp=0;
    int msg_id=657;
    bool got_alpha=false;
    bool slow_start_passed=SLOW_START_INIT_STATE;
    int slow_start_tran_step=0;
    int slow_start_counter=0;
    for(int i=0;i<FLOW_NUM;i++)
    {
        if (setsockopt(sock_for_cnt[i], IPPROTO_TCP, TCP_NODELAY, &reuse, sizeof(reuse)) < 0)
        {
            DBGERROR("ERROR: set TCP_NODELAY option %s\n",strerror(errno));
            return((void *)0);
        }
        //Enable DeepCC Style pacing on this socket:
#ifdef EVALUATION        
        int enable_deepcc=1;
        if(PACE_ENABLE==1)
            //With Pacing
            enable_deepcc=2;
        if (setsockopt(sock_for_cnt[i], IPPROTO_TCP, TCP_DEEPCC_ENABLE, &enable_deepcc, sizeof(enable_deepcc)) < 0) 
        {
            DBGERROR("CHECK KERNEL VERSION (0514+) ;CANNOT ENABLE DEEPCC %s\n",strerror(errno));
            return ((void* )0);
        }
#endif
#ifdef EVALUATION
        int deepcc_pacing_type=PACE_TYPE;
        if (setsockopt(sock_for_cnt[i], IPPROTO_TCP, TCP_DEEPCC_PACING_TYPE, &deepcc_pacing_type, sizeof(deepcc_pacing_type)) < 0)
        {
            DBGERROR("CHECK KERNEL VERSION (006+) ;CANNOT SET PACING TYPE %s\n",strerror(errno));
            return ((void* )0);
        }
        int deepcc_pacing_coef=PACE_COEF;
        if (setsockopt(sock_for_cnt[i], IPPROTO_TCP, TCP_DEEPCC_PACING_COEF, &deepcc_pacing_coef, sizeof(deepcc_pacing_coef)) < 0)
        {
            DBGERROR("CHECK KERNEL VERSION (006+) ;CANNOT SET PACING COEF %s\n",strerror(errno));
            return ((void* )0);
        }
#endif

    }
    char message[1000];
    char* shared_memory_rl2=  (char*)malloc(strlen(shared_memory_rl));
    char message_summarized[1000];
    char *num;
    char*alpha;
    char*save_ptr;
    int got_no_zero=0;
    uint64_t t0,t1,t2,t3,diff_fairness_time,t4,t5,t1_pre,start_time,time_tmp;
    t2=timestamp();

    //Time to start the Logic
    struct tcp_deepcc_info tcp_info_pre;
    tcp_info_pre.init();
    int get_info_error_counter=0;
    int actor_is_dead_counter=0;
    int tmp_step=0;
    int fairness_phase_cnt=0;

    //FILE *measurement_file;
    char file_name[1000];
    //sprintf(file_name,"%s/../dataset/%s_%s_cwnd.txt",path,scheme,log_file);
    //ofstream measurement_file(file_name);

    unsigned int sage_info_length = sizeof(sage_info);
    u64 total_bytes_acked_pre=0,max_delivary_rate=0,pre_max_delivary_rate=0,min_rtt_us=1,pre_bytes_sent=0,pre_pkt_dlv=0,pre_pkt_lost=0,pre_rtt_=0;
    u64 max_sending_rate=0;
    u32 pre_cwnd=CWND_INIT;
    double max_rate=0, delta_max_rate=0,pre_max_rate=0;
    u32 max_cwnd_effective=MAX_CWND_EFF_INIT;
    u32 max_cwnd_eff_srtt=MAX_CWND_EFF_INIT;
    double my_q_delay=0;
    double others_q_delay=0; 
    double reward=0.0;
    double acked_sent_rate = 0;

    dq_sage<u64>  sending_rates(WIN_SIZE*10);
    dq_sage<u64>  delivery_rates(WIN_SIZE*10);
    dq_sage<u64>  rtts(WIN_SIZE);
    
    dq_sage<double> ack_snt_s(SHORT_WIN);
    dq_sage<double> ack_snt_m(MID_WIN);
    dq_sage<double> acl_snt_l(LONG_WIN);

    dq_sage<double> rtt_s(SHORT_WIN);
    dq_sage<double> rtt_m(MID_WIN);
    dq_sage<double> rtt_l(LONG_WIN);

    dq_sage<double> lost_s(SHORT_WIN);      //x = lost pkts / 1000
    dq_sage<double> lost_m(MID_WIN);
    dq_sage<double> lost_l(LONG_WIN);

    dq_sage<double> inflight_s(SHORT_WIN); // x = inflight pkts / 1000
    dq_sage<double> inflight_m(MID_WIN);
    dq_sage<double> inflight_l(LONG_WIN);

    dq_sage<double> delivered_s(SHORT_WIN); 
    dq_sage<double> delivered_m(MID_WIN);
    dq_sage<double> delivered_l(LONG_WIN);

    dq_sage<double> thr_s(SHORT_WIN);
    dq_sage<double> thr_m(MID_WIN);
    dq_sage<double> thr_l(LONG_WIN);

    dq_sage<double> rtt_rate_s(SHORT_WIN);
    dq_sage<double> rtt_rate_m(MID_WIN);
    dq_sage<double> rtt_rate_l(LONG_WIN);

    dq_sage<double> rtt_var_s(SHORT_WIN);
    dq_sage<double> rtt_var_m(MID_WIN);
    dq_sage<double> rtt_var_l(LONG_WIN);

    dq_sage<double>  loss_win(50);
    dq_sage<double>  delta_time_win(50);

    dq_sage<u64>  sent_db(100);
    dq_sage<u64>  sent_dt(100);
    dq_sage<u64>  dlv_db(100);
    dq_sage<u64>  dlv_dt(100);
    dq_sage<u64>  loss_db(100);
    dq_sage<u64>  uack_db(100);

    double sr_w_mbps=0.0,dr_w_mbps=0.0,l_w_mbps=0.0,pre_dr_w_mbps=0;
    //
    dq_sage<double>  dr_w(200);     //2 Seconds?    //dr_w(4000); //40 Seconds
    dq_sage<double>     rtt_w(200);    //2 Seconds?

    double pre_dr_w_max=1.0;
    double dr_w_max=1.0;

    t0=timestamp();
    t1_pre=t0;
    double cwnd_precise=pre_cwnd;
    double cwnd_rate=1.0;
    
    u32 pre_lost_packets=0;
    double real_lost_rate = 0.0;
    start_time=timestamp();
    u64 total_lost =0;

    bool first_time_hitting_max = 0;

    u64 dt=0; 
    u64 dt_pre=timestamp();
    while(send_traffic)  
	{
       for(int i=0;i<flow_index;i++)
       {
           if(cc_state==state_DRL)
           {             
               got_no_zero=0;
               t3=timestamp();
               while(!got_no_zero && send_traffic)
               {
                    t1=timestamp();
                    if((t1-t0)<(report_period*1000))
                    {
                        usleep(report_period*1000-t1+t0);
                    }
                    ret1= getsockopt( sock_for_cnt[i], SOL_TCP, TCP_INFO, (void *)&sage_info, &sage_info_length);
                    if(ret1<0)
                    {
                        DBGMARK(0,0,"setsockopt: for index:%d flow_index:%d TCP_INFO ... %s (ret1:%d)\n",i,flow_index,strerror(errno),ret1);
                        return((void *)0);
                    }

                    int bytes_acked_ = (sage_info.bytes_acked-total_bytes_acked_pre);
                    u64 s_db;
                    u64 d_dp = (sage_info.delivered-pre_pkt_dlv);   //Delivered Delta Packets
                    u64 d_db = (d_dp)*sage_info.snd_mss;            //Delivered Delta Bytes
                    u64 l_db;
                    pre_pkt_dlv = sage_info.delivered; 

                    if (bytes_acked_>0 || d_db>0)
                    {
                        dt = timestamp()- dt_pre;
                        dt = (dt>0)?dt:1;
                        dt_pre = timestamp();
                        s_db = sage_info.bytes_sent-pre_bytes_sent;                 //Sent Delta Bytes
                        l_db = (sage_info.lost>pre_pkt_lost)?(sage_info.lost - pre_pkt_lost)*sage_info.snd_mss:0;
                        pre_pkt_lost = sage_info.lost;
                        uack_db.add((u64)sage_info.unacked*sage_info.snd_mss);
                        sent_db.add(s_db);
                        dlv_db.add(d_db);
                        loss_db.add(l_db);
                        sent_dt.add(dt);
                        dlv_dt.add(dt);  //It is redundant!
                        u64 dt_sum = sent_dt.sum();
                        sr_w_mbps = (double)8*sent_db.sum()/dt_sum;
                        dr_w_mbps = (double)8*dlv_db.sum()/dt_sum;
                        l_w_mbps  = (double)8*loss_db.sum()/dt_sum;
                        dr_w.add(dr_w_mbps);
                        dr_w_max = dr_w.max();
                        if (dr_w_max==0.0)
                            dr_w_max = 1;
                        rtt_w.add((double)sage_info.rtt/100000.0);
                         
                        double now=timestamp();
                        double time_delta=(double)(now-t0)/1000000.0;
                        //we divide to mss instead of mss+60! To make sure that we always have the right integer value!
                        //double acked_rate = bytes_acked_/(sage_info.snd_mss);
                        total_bytes_acked_pre = sage_info.bytes_acked;
                        double acked_rate = bytes_acked_;
                            acked_rate = acked_rate/time_delta;

                        now=now/1000000.0;
                        double sending_rate = (double)(sage_info.bytes_sent-pre_bytes_sent); //= (double)sage_info.snd_cwnd;
                        pre_bytes_sent = sage_info.bytes_sent;

                        double rtt_rate;                      
                        delivery_rates.add(sage_info.delivery_rate);                     
                        
                        max_delivary_rate = dr_w_max*1e6/8;

                        min_rtt_us = sage_info.min_rtt;

                        if(max_delivary_rate>0)
                        {
                            max_rate=(double) sage_info.delivery_rate/max_delivary_rate;
                            double tmp;
                            int tmp_;
                            tmp = (double) (max_delivary_rate/(sage_info.snd_mss+60))*(sage_info.rtt)/1e6;
                            max_cwnd_eff_srtt = (u32)tmp;
                            if(PACE_TYPE==PACE_WITH_MIN_RTT)
                            {
                                tmp = (double) (max_delivary_rate/(sage_info.snd_mss+60))*(min_rtt_us)/1e6;
                            }
                            tmp_ = (int)tmp;
                            max_cwnd_effective = tmp_;
                     
                            my_q_delay = (double)(1e6*sage_info.snd_cwnd*(sage_info.snd_mss+60)/max_delivary_rate);
                            others_q_delay = (double)(my_q_delay/min_rtt_us);                          
                            
                            if(max_cwnd_effective)
                                lost_rate = (double) (sage_info.lost)/max_cwnd_effective;                     
                                                         
                            if(sage_info.lost>pre_lost_packets)
                            {
                                loss_win.add((double) (8*abs((int)(sage_info.lost-pre_lost_packets))*sage_info.snd_mss)/1e6);                                                                                    
                                total_lost += 8*abs((int)(sage_info.lost-pre_lost_packets))*sage_info.snd_mss;
                            }
                            else
                            {                                                      
                                loss_win.add(0.0);
                            }
                            delta_time_win.add(time_delta);

                            real_lost_rate = 0.0;
                            double total_time_win_loss = delta_time_win.sum();
                            if(total_time_win_loss>0.0)
                            {
                                real_lost_rate = loss_win.sum()/total_time_win_loss; //Mbps
                            }

                            pre_lost_packets = sage_info.lost;
                        }
                        else
                        {
                             acked_rate = 0; 
                             lost_rate = 0;
                             pre_lost_packets = 0;
                             real_lost_rate = 0;
                        }
                        if(pre_max_delivary_rate>0)
                        {
                            delta_max_rate = (double) max_delivary_rate/pre_max_delivary_rate;
                        }
                        else
                        {
                            delta_max_rate = 0;
                        }
                        pre_max_delivary_rate = max_delivary_rate;
              
                        if(sage_info.rtt>0)
                        {
                            sending_rate = sending_rate/time_delta; // Bps

                            sending_rates.add((u64)sending_rate);
                            max_sending_rate = sending_rates.max();
                            //rtt_rate = (double)sage_info.min_rtt/sage_info.rtt;
                            rtt_rate = (double)min_rtt_us/sage_info.rtt;
                            //time_delta = sage_info.min_rtt/(1000000*time_delta);
                            time_delta = (1000000*time_delta)/sage_info.min_rtt;

                            acked_rate = acked_rate/max_sending_rate;
                            //lost_rate = lost_rate/max_sending_rate;
                        }
                        else
                        {
                            time_delta =report_period;
                            sending_rate = 0;
                            rtt_rate = 0.0;
                        }

                        double diff_rate ;
                        //if (sage_info.delivery_rate>0)
                        if (max_sending_rate>0)
                            diff_rate = (double)sage_info.delivery_rate/max_sending_rate;
                        else
                            diff_rate = 0;

                        time_tmp = timestamp()-send_begun;
                        double tmp_deliv_rate_mbps = (double)(sage_info.snd_mss*sage_info.delivered*8)/time_tmp;

                        double real_lost_rate_norm = 0;
                        if(max_delivary_rate)
                        {
                            real_lost_rate_norm = 1e6*real_lost_rate/max_delivary_rate/8; //signal = loss
                            max_rate = (double)sage_info.delivery_rate/max_delivary_rate;
                        }
                        else{
                            real_lost_rate_norm =0;
                            max_rate =0;
                        }

                        pre_max_rate = max_rate;

                        u64 s_db_tmp =sent_db.sum();
                        u64 ua_db_tmp = uack_db.avg();
                        double cwnd_unacked_rate = (s_db_tmp>0)?(double)ua_db_tmp/s_db_tmp:(double)ua_db_tmp; //(double)sage_info.unacked/sage_info.snd_cwnd;

                        double srate = (double)8*sage_info.bytes_sent/time_tmp;
                        double drate = (double)8*sage_info.delivered*sage_info.snd_mss/time_tmp;
                        double lrate = (double)total_lost/time_tmp; //Mbps
                        u64 cwnd_bits = (u64) sage_info.snd_cwnd*sage_info.snd_mss*8;
                        if(cwnd_bits==0)
                            cwnd_bits++;

//-------------------------------------------------------- Reward Calculation --------------------------------------------------------
                        //FIXME: Reward should be a function of time:
                        int time_on_trace = (int)(raw_timestamp()/1000-start_timestamp);
                        int time_on_trace_indx = ((time_on_trace/1000)%(trace_period*2))/(trace_period);
                        int bw1 = env_bw;
                        int bw_current_max = (time_on_trace_indx)?bw2:env_bw;
                        double max_tmp = (double)bw_current_max;
                        double bw_true_value = dr_w_mbps;

                        if (flows_num==1)
                        {
                            double rtt_rate_tmp=(rtt_rate>=0.8)?1:rtt_rate; 
                            int signof_bw=1;
                            if(dr_w_mbps<2*l_w_mbps) 
                            {
                                signof_bw=-1;
                            }
                            if(bw_true_value>(bw_current_max))          //transition to low value (e.g. as in 2x-d)
                            {
                                bw_true_value = bw_current_max*0.9;                           
                            }
                            reward = signof_bw*(25*(bw_true_value-2*l_w_mbps)*(bw_true_value-2*l_w_mbps))/(max_tmp*max_tmp)*rtt_rate_tmp;                          
                         }
                        else
                        {
                            // v0.810: 
                            // Our calculation of bw is about 5% off: 
                            // 98Mbps -> 96.2Mbps,   48Mbps -> 44.2Mbps
                            // So, b.c here reward is sensitive to the true value of bw, we apply a 5% correction to the calculations:
                            // fair_bw = 0.95 * max_tmp/flows_num
                            max_tmp = max_tmp * 0.95;
                            double fair_share = max_tmp/flows_num;
                            double loss_norm = (l_w_mbps<0.05*max_tmp)?0:l_w_mbps/max_tmp;
                            double x_norm = (dr_w_mbps>2.0*fair_share)?2.0:(fair_share>0.0)?(dr_w_mbps/fair_share):0.0; 
                            // Normal Distribution with mu=1, sigma=0.25, normalized to maximum(@x=1):
                            // 25 * e^(-0.5*((x-1)/sigma)^2) = 25 * e^(-8*(x-1)^2)
                            double param;
                            param = -8.0*(x_norm-1.0)*(x_norm-1.0);
                            reward = 25*exp(param);
                           
                        }
//---------------------------------------------------------------------------------------------------------------------------------------
                        /***
                        * Smoothed Versions ...
                        ***/
                        rtt_s.add((double)sage_info.rtt/100000);
                        rtt_m.add((double)sage_info.rtt/100000);
                        rtt_l.add((double)sage_info.rtt/100000);
                        
                        rtt_rate_s.add(rtt_rate);
                        rtt_rate_m.add(rtt_rate);
                        rtt_rate_l.add(rtt_rate);

                        rtt_var_s.add((double)sage_info.rttvar/1000.0);
                        rtt_var_m.add((double)sage_info.rttvar/1000.0);
                        rtt_var_l.add((double)sage_info.rttvar/1000.0);

                        thr_s.add((double)sage_info.delivery_rate/125000.0/BW_NORM_FACTOR);
                        thr_m.add((double)sage_info.delivery_rate/125000.0/BW_NORM_FACTOR);
                        thr_l.add((double)sage_info.delivery_rate/125000.0/BW_NORM_FACTOR);
                         
                        inflight_s.add((double)sage_info.unacked/1000.0);
                        inflight_m.add((double)sage_info.unacked/1000.0);
                        inflight_l.add((double)sage_info.unacked/1000.0);
                    
                        lost_s.add((double)sage_info.lost/100.0);
                        lost_m.add((double)sage_info.lost/100.0); 
                        lost_l.add((double)sage_info.lost/100.0);

                        /**
                        * Soheil: Other interesting fields: potential input state candidates
                        * */
                       char message_extra[1000];
                       sprintf(message_extra,
                               "     %.7f %.7f %.7f %.7f   %.7f %.7f    %d %u    %.7f %.7f %.7f   %.7f %.7f %.7f   %.7f %.7f %.7f    %.7f %.7f %.7f    %.7f %.7f %.7f    %.7f %.7f %.7f    %.7f %.7f %.7f   %.7f %.7f %.7f   %.7f %.7f %.7f    %.7f %.7f %.7f    %.7f %.7f %.7f    %.7f %.7f %.7f     %.7f %.7f %.7f    %.7f %.7f %.7f    %.7f %.7f %.7f       %.7f %.7f %.7f    %.7f %.7f %.7f    %.7f %.7f %.7f                     ",

                               //2
                               (double)sage_info.rtt/100000.0, /*sRTT in 100x (ms):e.g. 2 = 2x100=200 ms*/
                               //3
                               (double)sage_info.rttvar/1000.0,    /*var of sRTT in 1x (ms). */
                               //4
                               (double)sage_info.rto/100000.0,  /*retrans timeout in 100x (ms)*/
                               //5
                               (double)sage_info.ato/100000.0,  /*Ack timeout in 100x (ms)*/
                               //6
                               (double)sage_info.pacing_rate/125000.0/BW_NORM_FACTOR, /*pacing rate 100x Mbps*/
                               //7
                               (double)sage_info.delivery_rate/125000.0/BW_NORM_FACTOR, /*del rate 100x Mbps*/
                               //8 
                               sage_info.snd_ssthresh,
                               //9
                               sage_info.ca_state,             /*TCP_CA_Open=0 -> TCP_CA_Loss=4*/

                               //10, 11, 12     13, 14, 15   16, 17, 18
                               rtt_s.get_avg(),rtt_s.get_min(),rtt_s.get_max(),
                               rtt_m.get_avg(),rtt_m.get_min(),rtt_m.get_max(),
                               rtt_l.get_avg(),rtt_l.get_min(),rtt_l.get_max(),
                               //19, 20, 21     22, 23, 24   25, 26, 27
                               thr_s.get_avg(),thr_s.get_min(),thr_s.get_max(),
                               thr_m.get_avg(),thr_m.get_min(),thr_m.get_max(),
                               thr_l.get_avg(),thr_l.get_min(),thr_l.get_max(),
                               //28, 29, 30     31, 32, 33   34, 35, 36
                               rtt_rate_s.get_avg(),rtt_rate_s.get_min(),rtt_rate_s.get_max(),
                               rtt_rate_m.get_avg(),rtt_rate_m.get_min(),rtt_rate_m.get_max(),
                               rtt_rate_l.get_avg(),rtt_rate_l.get_min(),rtt_rate_l.get_max(),
                               //37, 38, 39     40, 41, 42   43, 44, 45
                               rtt_var_s.get_avg(),rtt_var_s.get_min(),rtt_var_s.get_max(),
                               rtt_var_m.get_avg(),rtt_var_m.get_min(),rtt_var_m.get_max(),
                               rtt_var_l.get_avg(),rtt_var_l.get_min(),rtt_var_l.get_max(),
                               //46, 47, 48     49, 50, 51   52, 53, 54                              
                               inflight_s.get_avg(),inflight_s.get_min(),inflight_s.get_max(),
                               inflight_m.get_avg(),inflight_m.get_min(),inflight_m.get_max(),
                               inflight_l.get_avg(),inflight_l.get_min(),inflight_l.get_max(), 
                               //55, 56, 57     58, 59, 60   61, 62, 63
                               lost_s.get_avg(),lost_s.get_min(),lost_s.get_max(),
                               lost_m.get_avg(),lost_m.get_min(),lost_m.get_max(),
                               lost_l.get_avg(),lost_l.get_min(),lost_l.get_max() 
                               );
                                 
                        if(DS_VERSION>500)
                        {
                           sprintf(message,"%.7f %.7f %s  %.7f   %.7f %.7f   %.7f %.7f    %.7f %.7f    %.7f %.7f   %.7f %.7f   %.7f %.7f",
                            //0                           1        [2-63] 
                            (double)time_on_trace/1e3, max_tmp, message_extra,
                            //64
                            dr_w_mbps-l_w_mbps,
                            //65          66
                            time_delta,rtt_rate, 
                            //67                        68
                            l_w_mbps/BW_NORM_FACTOR,acked_rate,
                            //69                                                            70
                            (pre_dr_w_mbps>0.0)?(dr_w_mbps/pre_dr_w_mbps):dr_w_mbps,(double)(dr_w_max*sage_info.min_rtt)/(cwnd_bits),
                            //71                                    72
                            (dr_w_mbps)/BW_NORM_FACTOR, cwnd_unacked_rate, //sr_w_mbps/dr_w_max,
                            //73
                            (pre_dr_w_max>0.0)?(dr_w_max/pre_dr_w_max):dr_w_max,
                            //74
                            dr_w_max/BW_NORM_FACTOR,
                            //Soheil: If we should change the reward (giving pentalties, etc.), 
                            //we will do it here and send the final reward to python code. 
                            //75    76
                            reward,(cwnd_rate>0.0)?round(log2f(cwnd_rate)*1000)/1000.:log2f(0.0001));
                        }
                        else
                        {
                            sprintf(message,"%.7f %.7f   %.7f   %.7f %.7f   %.7f %.7f    %.7f %.7f    %.7f %.7f   %.7f %.7f    %.7f %.7f",
                            (double)time_on_trace/1e3, 
                            max_tmp,
                            dr_w_mbps-l_w_mbps,
                            time_delta,rtt_rate, 
                            l_w_mbps/BW_NORM_FACTOR,acked_rate,
                            (pre_dr_w_mbps>0.0)?(dr_w_mbps/pre_dr_w_mbps):dr_w_mbps,(double)(dr_w_max*sage_info.min_rtt)/(cwnd_bits),
                            (dr_w_mbps-l_w_mbps)/BW_NORM_FACTOR, cwnd_unacked_rate, //sr_w_mbps/dr_w_max,
                            (pre_dr_w_max>0.0)?(dr_w_max/pre_dr_w_max):dr_w_max,
                            dr_w_max/BW_NORM_FACTOR,                          
                            reward,(cwnd_rate>0.0)?round(log2f(cwnd_rate)*1000)/1000.:log2f(0.0001));
                        }
                            
                        sprintf(message_summarized,"bw:%.3f   rtt:%.3f   loss:%.3f srate:%f max:%.3f  r:%.3f a:%.7f",
                            dr_w_mbps,
                            rtt_rate, 
                            l_w_mbps/BW_NORM_FACTOR,
                            (double)sage_info.pacing_rate/125000.0/BW_NORM_FACTOR, /*pacing rate 100x Mbps*/
                            max_tmp,
                            reward,(cwnd_rate>0.0)?round(log2f(cwnd_rate)*1000)/1000.:log2f(0.0001));

#ifdef EVALUATION   
                        char message2[1000];
                        sprintf(message2,"%d %s",msg_id,message);
                        memcpy(shared_memory,message2,sizeof(message2));
#else
                        measurement_file << message<<"\n";
#endif
                        got_no_zero=1;
                        t0=timestamp();
                        pre_rtt_ = sage_info.rtt;
                        pre_dr_w_mbps = dr_w_mbps;
                        pre_dr_w_max = dr_w_max;
                        msg_id=(msg_id+1)%1000;
                    }
                }
                //
#ifdef EVALUATION                
                got_alpha=false;
                int error_cnt=0;
                int error2_cnt=0;
                double target_cwnd=4;
                u64 cwnd_tmp=0;
                u64 cwnd_max_tmp=0;
                int cnt_tmp = 0;
                int tmp___=0;
                int show_log=0;
                u64 action_get_time_start_ms = timestamp()/1000;
                while(!got_alpha && send_traffic)
                { 
                   //Get alpha from RL-Module
                   strcpy(shared_memory_rl2,shared_memory_rl);  //to not destroy the original string
                   num=strtok_r(shared_memory_rl2," ",&save_ptr);

                   alpha=strtok_r(NULL," ",&save_ptr);
                   if(num!=NULL && alpha!=NULL)
                   {
                       if (show_log)                            
                       {
                           DBGERROR("Time: %f (s), still waiting got previous one ...\n",(double)(raw_timestamp()/1000-start_timestamp)/1000);
                           show_log=0;
                       }
                       pre_id_tmp=atoi(num);
                       if(pre_id!=pre_id_tmp /*&& target_ratio!=OK_SIGNAL*/)
                       {
                          got_alpha=true; 
                          cnt_tmp=0;
                          pre_id=pre_id_tmp; 
                          target_cwnd = atof(alpha);
                          if(ESTIMATE)
                          {
                              double tmp_target_cwnd = ACCURACY*target_cwnd;
                              u64 alpha_tmp = round(tmp_target_cwnd);
                              target_cwnd = alpha_tmp/ACCURACY;
                          }
                     
                          if(!TURN_ON_SAFETY)
                          {
                                if(ACCUMULATE_CWND)
                                {
                                    cwnd_precise = mul(cwnd_precise,target_cwnd);
                                    if(ROUNT_TYPE==FLOORING)
                                    {
                                        cwnd_tmp = floor(cwnd_precise);
                                    }
                                    else if (ROUNT_TYPE==CEILING)
                                    {
                                        cwnd_tmp = ceil(cwnd_precise);
                                    }
                                    else if (ROUNT_TYPE==ROUNDING)
                                    {
                                        cwnd_tmp = round(cwnd_precise);
                                    }
                                    else
                                    {
                                        cwnd_tmp = (u64)(cwnd_precise);
                                    }
                                    if(cwnd_tmp>=MAX_32Bit)
                                        target_ratio = MAX_32Bit;
                                    else
                                        target_ratio = cwnd_tmp;
                                }
                                else
                                { 
                                    target_ratio = floor(mul((double)sage_info.snd_cwnd,target_cwnd));                                  
                                }
                          }
                          
                          if(target_ratio<MIN_CWND)
                          {
                              target_ratio = MIN_CWND;
                              cwnd_precise = MIN_CWND;
                          }
                    
                          if(target_ratio>MAX_CWND)
                          {
                              target_ratio = MAX_CWND;
                              cwnd_precise = MAX_CWND;
                          }

                          //DBGERROR("T:%.3f id: %d Cwnd: %d   %s\n",(double)(raw_timestamp()/1000-start_timestamp)/1000,actor_id,target_ratio,message_summarized);
                          ret1 = setsockopt(sock_for_cnt[i], IPPROTO_TCP,TCP_CWND, &target_ratio, sizeof(target_ratio));
                          if(ret1<0)
                          {
                              DBGERROR("setsockopt: for index:%d flow_index:%d ... %s (ret1:%d)\n",i,flow_index,strerror(errno),ret1);
                              return((void *)0);
                          }
                          cwnd_rate = target_cwnd; 
                          error_cnt=0;
                       }
                       else{
                           if (error_cnt>600000)
                           {
                               DBGERROR("After 1 Minute, stilll no new value id:%d prev_id:%d, Server is going down down down\n",pre_id_tmp,pre_id);
                               send_traffic=false;
                               error_cnt=0;
                           }
                           error_cnt++;
                           usleep(10);
                           if (error_cnt%10000 ==0)
                               DBGERROR("---------Actor_id: %d, PreID: %d\n", actor_id, pre_id);
                       }

                       error2_cnt=0;
                   }
                   else{
                        usleep(10);
                        if (error2_cnt%1000 == 0)
                            DBGERROR("NULL-ALPHA => cnt=%d! Actor_id: %d, PreID: %d num:%s alpha:%s\n", error2_cnt, actor_id, pre_id, num, alpha);
                        error2_cnt += 1;
                   }
                   if(!got_alpha && (timestamp()/1000-action_get_time_start_ms)>WAIT_FOR_ACTION_MAX_ms)
                   {
                       DBGERROR("............. Time: %f (s), No action from RL agent (id:%d), skipping this round ................\n",(double)(raw_timestamp()/1000-start_timestamp)/1000,pre_id);
                       //Skip this round!
                       got_alpha=1;
                   }
                }
#endif
           }
           else
           {
                target_ratio=4;
                ret1 = setsockopt(sock_for_cnt[i], IPPROTO_TCP,TCP_CWND, &target_ratio, sizeof(target_ratio));
                if(ret1<0)
                {
                  DBGERROR("setsockopt: for index:%d flow_index:%d ... %s (ret1:%d)\n",i,flow_index,strerror(errno),ret1);
                  return((void *)0);
                }
                usleep(report_period);
                t5=timestamp();
                double slient_per_is_done=(double)((t5-t4)/100000.0);      
                //100ms passed?
                if(slient_per_is_done>1.0)
                {
                    cc_state=state_DRL;
                }
           }

       }
#ifdef EVALUATION
       iterations++;
       step_it++;
       if ((duration_steps!=0))
       {
           if (iterations>duration_steps)    
           {
               send_traffic=false;
           }
       }
#endif

    }
#ifdef EVALUATION
    shmdt(shared_memory);
    shmctl(shmid, IPC_RMID, NULL);
    shmdt(shared_memory_rl);
    shmctl(shmid_rl, IPC_RMID, NULL);
    free(shared_memory_rl2);
#endif
    //fclose(measurement_file);
    //measurement_file.close();

    return((void *)0);
}
void* DataThread(void* info)
{
    /*
	struct sched_param param;
    param.__sched_priority=sched_get_priority_max(SCHED_RR);
    int policy=SCHED_RR;
    int s = pthread_setschedparam(pthread_self(), policy, &param);
    if (s!=0)
    {
        DBGERROR("Cannot set priority (%d) for the Main: %s\n",param.__sched_priority,strerror(errno));
    }

    s = pthread_getschedparam(pthread_self(),&policy,&param);
    if (s!=0)
    {
        DBGERROR("Cannot get priority for the Data thread: %s\n",strerror(errno));
    }*/

	cFlow* flow = (cFlow*)info;
	int sock_local = flow->flowinfo.sock;
	char* src_ip;
	char write_message[BUFSIZ+1];
	char read_message[1024]={0};
	int len;
	char *savePtr;
	char* dst_addr;
	u64 loop;
	u64  remaining_size;

	memset(write_message,1,BUFSIZ);
	write_message[BUFSIZ]='\0';
	/**
	 * Get the RQ from client : {src_add} {flowid} {size} {dst_add}
	 */
	len=recv(sock_local,read_message,1024,0);
	if(len<=0)
	{
		DBGMARK(DBGSERVER,1,"recv failed! \n");
		close(sock_local);
		return 0;
	}
	/**
	 * For Now: we send the src IP in the RQ too!
	 */
	src_ip=strtok_r(read_message," ",&savePtr);
	if(src_ip==NULL)
	{
		//discard message:
		DBGMARK(DBGSERVER,1,"id: %d discarding this message:%s \n",flow->flowinfo.flowid,savePtr);
		close(sock_local);
		return 0;
	}
	char * isstr = strtok_r(NULL," ",&savePtr);
	if(isstr==NULL)
	{
		//discard message:
		DBGMARK(DBGSERVER,1,"id: %d discarding this message:%s \n",flow->flowinfo.flowid,savePtr);
		close(sock_local);
		return 0;
	}
	flow->flowinfo.flowid=atoi(isstr);
	char* size_=strtok_r(NULL," ",&savePtr);
	flow->flowinfo.size=1024*atoi(size_);
    DBGPRINT(DBGSERVER,4,"%s\n",size_);
	dst_addr=strtok_r(NULL," ",&savePtr);
	if(dst_addr==NULL)
	{
		//discard message:
		DBGMARK(DBGSERVER,1,"id: %d discarding this message:%s \n",flow->flowinfo.flowid,savePtr);
		close(sock_local);
		return 0;
	}
	char* time_s_=strtok_r(NULL," ",&savePtr);
    char *endptr;
    start_of_client=strtoimax(time_s_,&endptr,10);
	got_message=1;
    DBGPRINT(DBGSERVER,2,"Got message: %" PRIu64 " us\n",timestamp());
    flow->flowinfo.rem_size=flow->flowinfo.size;
    DBGPRINT(DBGSERVER,2,"time_rcv:%" PRIu64 " get:%s\n",start_of_client,time_s_);

	//Get detailed address
	strtok_r(src_ip,".",&savePtr);
	if(dst_addr==NULL)
	{
		//discard message:
		DBGMARK(DBGSERVER,1,"id: %d discarding this message:%s \n",flow->flowinfo.flowid,savePtr);
		close(sock_local);
		return 0;
	}

	//////////////////////////////////////////////////////////////////////////////
	//Calculate loops. In each loop, we can send BUFSIZ (8192) bytes of data
	loop=flow->flowinfo.size/BUFSIZ*1024;
	//Calculate remaining size to be sent
	remaining_size=flow->flowinfo.size*1024-loop*BUFSIZ;
	//Send data with 8192 bytes each loop
    //DBGPRINT(DBGSERVER,5,"size:%" PRId64 "\trem_size:%u,loop:%" PRId64 "\n",flow->flowinfo.size*1024,remaining_size,loop);
	//DBGERROR("Server is sending the traffic ...\n");
    send_begun = timestamp();
    //fprintf(testing_,"Took %f ms to start sending Date packets\n",(double)(raw_timestamp()/1000-start_timestamp));
    //fclose(testing_);
    while(send_traffic)
    {
        len=strlen(write_message);
		while(len>0)
		{
			DBGMARK(DBGSERVER,7,"++++++%d\n",send_traffic);
			len-=send(sock_local,write_message,strlen(write_message),0);
		    //usleep(50);         
            usleep(1);         
            DBGMARK(DBGSERVER,7,"      ------\n");
		}
        //usleep(100);
        usleep(2);
	}
    flow->flowinfo.rem_size=0;
    done=true;
    DBGPRINT(DBGSERVER,1,"done=true\n");
    close(sock_local);
    DBGPRINT(DBGSERVER,1,"done\n");
	return((void *)0);
}
