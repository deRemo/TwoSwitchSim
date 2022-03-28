//Queue theory: Two switches in cascade can be represented as a network of two queues in tandem

#include <iostream>
#include <fstream>
#include<tuple>
#include <queue>
#include <deque>
#include "lcgrand.h"
#include <math.h> 

//System status mnemonics
inline const int IDLE = 0;
inline const int BUSY = 1;

//Event type mnemonics
inline const int A1 = 2;  //Arrival at queue 1
inline const int D1 = 3;  //Departure from queue 1 (includes an immediate arrival at queue 2)
inline const int D2 = 4;  //Departure from queue 2

//Queue names
inline const std::string Q1 = "Q1";
inline const std::string Q2 = "Q2";

//Events are represented as tuples <event_time, event_type>
typedef std::tuple<float, int> tup_t;

//Simulation variables
float sim_clock;
int   next_event_type;
int   q_limit;

//Useful information about the state of a queue
typedef struct q_info {
    std::string name;               //queue name
    std::deque<float> pending_pkts; //arrival time of currently waiting/delayed packets
    int n_pkts;                     //size of pending_pkts
    int status;                     //queue status (either BUSY or IDLE)

    q_info* next;                   //reference to the next queue
} q_info_t;

q_info_t q1;
q_info_t q2;
std::priority_queue<tup_t, std::vector<tup_t>, std::greater<tup_t>> event_list;

//System configuration
float mean_interarrival_time; 
float mean_service_time;
int   num_pkts;
int   seed;

//Stat counters
int   processed_pkts;
float total_queue_delay;  //Sum of all queue delays (=time waiting in queue)
float total_service;      //Sum of all service times 

void  init(void);                             //Initializes the simulation model and starts the simulation
void  timing(void);                           //Determines the next event and perform the (simulated) time advance
void  arrival_event(q_info_t* q);             //Arrival at queue 1 or 2 event routine
void  departure_event(q_info_t* q, 
                      int d_event);           //Departure from queue 1 or 2 event routine
void  report(void);                           //Generates report and print in output file
float expon(float mean);                      //Exponential variate generator (Inverse-Transform Method)
float trunc_expon(float mean, int a, int b);  //Doubly truncated exponential variate generator (Inverse-Transform Method)

void connect(q_info_t* q1, q_info_t* q2);     //Connect q1 to q2 (q1->q2)

template <typename T>
void print_q(std::deque<T> q){
    while (!q.empty() ) {
        std::cout << q.front() << "\n";
        q.pop_front();
    }
}

int main(){
    //Read system configuration and generator seed
    std::fstream istream("input.txt", std::ios::in);

    if (!istream.is_open()){
        std::cerr << "Couldn't open input file: does it exist?" << std::endl;
        return EXIT_FAILURE;
    }

    for (int i = 0; i <= 4; i++){
        if (istream.eof()){
            std::cerr << "Input file error: not enough parameters" << std::endl;
            return EXIT_FAILURE;
        }

        switch(i){
            case 0:
                istream >> mean_interarrival_time;
                break;
            case 1:
                istream >> mean_service_time;
                break;
            case 2:
                istream >> num_pkts;
                break;
            case 3:
                istream >> seed;
                break;
            case 4:
                istream >> q_limit;
                break;
        }
    }
    istream.close();

    init();

    while (processed_pkts < num_pkts){
        //std::cout << "processed_pkts: " << processed_pkts << std::endl;
        timing();
        
        switch (next_event_type){
        case A1:
            arrival_event(&q1);
            break;
        case D1:
            departure_event(&q1, D1);
            break;
        case D2:
            departure_event(&q2, D2);
            break;
        }
    }

    std::cout << "processed_pkts: " << processed_pkts << std::endl;
    report();

    return EXIT_SUCCESS;
}

void init(void) {
    sim_clock = 0;

    q1.name = Q1;
    q2.name = Q2;

    q1.n_pkts = 0;
    q2.n_pkts = 0;

    q1.status = IDLE;
    q2.status = IDLE;

    connect(&q1, &q2);
    q2.next = NULL;

    processed_pkts = 0;
    total_queue_delay = 0;
    total_service = 0;

    //Generate first event
    event_list.push(std::make_tuple(expon(mean_interarrival_time), A1));
    next_event_type = A1;
}

void timing(void) {
    if (event_list.empty()) {
        std::cerr << "Event list empty at (simulated) time " << sim_clock;
        exit(EXIT_FAILURE);
    }

    tup_t next_event = event_list.top();
    event_list.pop();

    sim_clock = std::get<0>(next_event);
    next_event_type = std::get<1>(next_event);
}

//schedule departure (= service completion) from q
void schedule_departure_event_from(q_info_t* q){
    //If next queue doesn't exist, the packet has been completely processed by the queue system
    if (!q->next){
        processed_pkts += 1;
    }

    float service_time = expon(mean_service_time);

    //Identify packet location in order to pair the departure time with the right event type
    int event_type = (!q->next) ? D2 : D1;

    event_list.push(std::make_tuple(sim_clock + service_time, event_type));
    total_service += service_time;
}

void arrival_event(q_info_t* q) {
    std::cout << "arrival   " << q->name << "  (" << sim_clock << ")" << std::endl;

    if (q->status == BUSY) { //Packet cannot be processed immediately, keep it pending
        q->n_pkts += 1;

        if (q->n_pkts > q_limit){
            std::cerr << q->name << " overflow at (simulated) time " << sim_clock << std::endl;
            exit(EXIT_FAILURE); 
        }

        q->pending_pkts.push_back(sim_clock);
    }
    else { //Process packet immediately (no queue delay)
        q->status = BUSY;
        schedule_departure_event_from(q);
    }

    //schedule next arrival (or else the simulation ends), but only for the first queue
    if(q->next){
        event_list.push(std::make_tuple(sim_clock + expon(mean_interarrival_time), A1));
    }
}

void departure_event(q_info_t* q, int d_event) {
    std::cout << "departure " << q->name << "  (" << sim_clock << ")" << std::endl;
    if (d_event != D1 && d_event != D2){
        std::cerr << "Error: unrecognized (departure) event type" << std::endl;
        exit(EXIT_FAILURE);
    }

    //finished serving one packet
    if (q->next) { //immediate arrival to the next queue (if any)
        arrival_event(q->next);
    }

    //start serving the first pending one (if any)
    if (q->n_pkts == 0) { //queue is empty, no pending packets
        q->status = IDLE;
    }
    else { //queue is not empty, thus start serving the next pending packet
        q->n_pkts -= 1;

        //compute and store the packet's queue delay
        total_queue_delay += sim_clock - q->pending_pkts.front();
        q->pending_pkts.pop_front();

        //schedule departure (= service completion) of the next pending packet
        schedule_departure_event_from(q);
    }
}

void report(void){
    std::fstream ostream("output.txt", std::ios::out | std::ios::trunc);
    if (!ostream.is_open()){
        std::cerr << "Couldn't open output file" << std::endl;
        exit(EXIT_FAILURE);
    }

    float avg_system_delay = (total_queue_delay + total_service) / processed_pkts;

    std::cout << "avg system delay: " << avg_system_delay << " time units" << std::endl;
    ostream << avg_system_delay;

    ostream.close();
}


//NOTE: "mean" is not a rate, but a time
float expon(float mean){ 
    return -mean * log(1 - lcgrand(seed));
}

float trunc_expon(float mean, int a, int b){
    return a - (log(1 - lcgrand(seed) * (1 - exp((a - b) / mean)))) * mean;
}


void connect(q_info_t* q1, q_info_t* q2){
    q1->next = q2;
}