//Queue theory: Two switches in cascade can be represented as a network of two queues in tandem

#include <iostream>
#include <fstream>
#include<tuple>
#include <queue>
#include <deque>
#include "lcgrand.h"
#include <math.h> 

//System status mnemonics
#define IDLE      0
#define BUSY      1

//Event type mnemonics
#define A1        2  //Arrival at queue 1
#define D1        3  //Departure from queue 1 (implies Arrival at queue 2)
#define D2        4  //Departure from queue 2
#define MACRONAME(x) #x

//Events are represented as tuples <event_time, event_type>
typedef std::tuple<float, int> tup_t;

//Simulation variables
float sim_clock;
int   next_event_type;
int   q_limit;


std::priority_queue<tup_t, std::vector<tup_t>, std::greater<tup_t>> event_list;

//Store useful information about the status of a queue
typedef struct q_info {
    std::string name;               //queue name
    std::deque<float> pending_pkts; //currently waiting/delayed packets
    int n_pkts;                     //size of pending_pkts
    int status;                     //queue status (either BUSY or IDLE)
} q_info_t;

q_info_t q1;
q_info_t q2;

std::deque<float> pending_q1;
std::deque<float> pending_q2;

//System configuration
float mean_interarrival_time; 
float mean_service_time;
int   num_pkts;
int   seed;

//System state variables
int pkts_in_q1;
int pkts_in_q2;

int q1_status;
int q2_status;

//Stat counters
int   processed_pkts;
float total_queue_delay;

void  init(void);                             //Initializes the simulation model and starts the simulation
void  timing(void);                           //Determines the next event and perform the (simulated) time advance
void  arrival_event(void);                    //Arrival at queue 1 event routine
void  departure_event(int event_type);        //Departure from queue 1 or 2 event routine
void  report(void);                           //Generates report and print in output file
float expon(float mean);                      //Exponential variate generator (Inverse-Transform Method)
float trunc_expon(float mean, int a, int b);  //Doubly truncated exponential variate generator (Inverse-Transform Method)

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
        timing();

        switch (next_event_type){
        case A1:
            arrival_event();
            break;
        case D1:
            departure_event(D1);
            break;
        case D2:
            departure_event(D2);
            break;
        }
    }

    std::cout << "processed_pkts: " << processed_pkts << std::endl;
    report();

    return EXIT_SUCCESS;
}

void init(void) {
    sim_clock = 0;

    q1.name = "q1";
    q2.name = "q2";

    q1.n_pkts = 0;
    q2.n_pkts = 0;

    q1.status = IDLE;
    q2.status = IDLE;

    processed_pkts = 0;
    total_queue_delay = 0;

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

void arrival_event(void) {
    std::cout << "arrival" << std::endl;

    if (q1.status == BUSY) { //Store in queue
        q1.n_pkts += 1;

        if (q1.n_pkts > q_limit){
            std::cerr << "Q1 overflow at (simulated) time " << sim_clock << std::endl;
            exit(EXIT_FAILURE); 
        }

        q1.pending_pkts.push_back(sim_clock);
    }
    else { //Process packet immediately (no queue delay)
        processed_pkts += 1;
        q1.status = BUSY;

        //schedule departure (= service completion) (= packet processed)
        event_list.push(std::make_tuple(sim_clock + expon(mean_service_time), D1));
    }

    //schedule next arrival
    event_list.push(std::make_tuple(sim_clock + expon(mean_interarrival_time), A1));
}

void departure_event(int event_type) {
    q_info_t* q;

    if (event_type == D1){
        q = &q1;
    }
    else if (event_type == D2){
        q = &q2;
    }
    else{
        std::cerr << "Error: unrecognized (departure) event type" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "departure " << q->name << std::endl;

    if (q->n_pkts == 0) {
        q->status = IDLE;
    }
    else {
        q->n_pkts -= 1;
        processed_pkts += 1;

        //compute and store the packet's queue delay
        total_queue_delay += sim_clock - q->pending_pkts.front();
        q->pending_pkts.pop_front();

        /*if (event_type == D1) {
            event_list.push(std::make_tuple(sim_clock + expon(mean_service_time), D2));
        }*/

        //schedule departure
        event_list.push(std::make_tuple(sim_clock + expon(mean_service_time), D1));
    }
}

void report(void){
    std::fstream ostream("output.txt", std::ios::out | std::ios::trunc);
    if (!ostream.is_open()){
        std::cerr << "Couldn't open output file" << std::endl;
        exit(EXIT_FAILURE);
    }

    float avg_system_delay = total_queue_delay / processed_pkts;

    std::cout << "avg system delay: " << avg_system_delay << std::endl;
    ostream << avg_system_delay;

    ostream.close();
}


//NOTE: "mean" is not a rate (actually, it is the inverse of that)

float expon(float mean){ 
    return -mean * log(1 - lcgrand(seed));
}

float trunc_expon(float mean, int a, int b){
    return a - (log(1 - lcgrand(seed) * (1 - exp((a - b) / mean)))) * mean;
}