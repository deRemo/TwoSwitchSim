//Queue theory: Two switches in cascade can be represented as a network of two queues in tandem

#include <iostream>
#include <fstream>
#include <sstream>
#include <tuple>
#include <map>
#include <queue>
#include <deque>
#include <math.h> 

#include "lcgrand.h"

static unsigned int current_id;    //Monotonically increasing queue id generator

//Events are represented as tuples < event_time, event_type, queue_id >
typedef std::tuple<float, int, unsigned int> tup_t;

//System status mnemonics
inline const int IDLE = 0;
inline const int BUSY = 1;

//Event type mnemonics
inline const int A = 2;  //Arrival (=customer arrival)
inline const int D = 3;  //Departure (=service completion)

//Simulation variables
float        sim_clock;
int          next_event_type;
unsigned int next_queue_id;
int          q_limit;

//Useful information about the state of a queue
class q_info_t {
public:
    unsigned int id;                //queue id
    std::string name;               //queue name
    std::deque<float> pending_pkts; //arrival time of currently waiting/delayed packets
    int n_pkts;                     //size of pending_pkts
    int status;                     //queue status (either BUSY or IDLE)
 
    float mean_service_time;

    q_info_t* next;                 //reference to the next queue in the system

    q_info_t(){
        id = ++current_id;
        name = "Q" + std::to_string(id);
        n_pkts = 0;
        status = IDLE;

        mean_service_time = -1;

        next = NULL;
    }
};

//Keep track of queues inside the network
std::map<unsigned int, q_info_t*> q_registry;

q_info_t q1;
q_info_t q2;
std::priority_queue<tup_t, std::vector<tup_t>, std::greater<tup_t>> event_list;

//System configuration
float mean_interarrival_time; 
float mean_service_time_1;
float mean_service_time_2;

float   a, b; //Doubly Truncated Neg. Expon. Distribution in [a,b]
int   num_pkts;
int   seed;

//Stat counters
int   processed_pkts;
float total_queue_delay;  //Sum of all queue delays (=time waiting in queue)
float total_service;      //Sum of all service times 

void  init(void);                             //Initializes the simulation model and starts the simulation
void  timing(void);                           //Determines the next event and perform the (simulated) time advance
void  arrival_event(q_info_t* q);             //Arrival at queue 1 or 2 event routine
void  departure_event(q_info_t* q);           //Departure from queue 1 or 2 event routine
void  report(void);                           //Generates report and print in output file
float expon(float mean);                      //Exponential variate generator (Inverse-Transform Method)
float trunc_expon(float mean, float a, float b);  //Doubly truncated exponential variate generator (Inverse-Transform Method)

void connect(q_info_t* q1, q_info_t* q2);     //Connect q1 to q2 (q1->q2)

int main(){
    //Read system configuration and generator seed
    //config file syntax: 'name=value' (one per line)
    std::fstream istream("input.txt", std::ios::in);
    std::string line;

    while (std::getline(istream, line)) {
        std::istringstream iss_line(line);
        std::string key;

        if (line[0] != '#' && std::getline(iss_line, key, '=')){
            std::string value;
            if (std::getline(iss_line, value)) {
                std::cout << key << " = " << value << std::endl;

                if (key == "mean_interarrival_time") {
                    mean_interarrival_time = std::stof(value);
                } else if (key == "mean_service_time_1") {
                    mean_service_time_1 = std::stof(value);
                } else if (key == "mean_service_time_2") {
                    mean_service_time_2 = std::stof(value);
                } else if (key == "a") {
                    a = std::stof(value);
                } else if (key == "b") {
                    b = std::stof(value);;
                } else if (key == "num_pkts") {
                    num_pkts = std::stoi(value);
                } else if (key == "seed") {
                    seed = std::stoi(value);
                } else if (key == "q_limit") {
                    q_limit = std::stoi(value);
                } else {
                    std::cout << "Unrecognized option" << key << std::endl;
                    return EXIT_FAILURE;
                }
            } 
        }
    }

    std::cout << std::endl;
    istream.close();

    init();

    while (processed_pkts < num_pkts){
        timing();
        
        switch (next_event_type){
        case A:
            arrival_event(q_registry[next_queue_id]);
            break;
        case D:
            departure_event(q_registry[next_queue_id]);
            break;
        }
    }

    std::cout << "processed_pkts: " << processed_pkts << std::endl;
    report();

    return EXIT_SUCCESS;
}

void init(void) {
    sim_clock = 0;
    current_id = 1;

    processed_pkts = 0;
    total_queue_delay = 0;
    total_service = 0;

    //Set user-defined service times
    q1.mean_service_time = mean_service_time_1;
    q2.mean_service_time = mean_service_time_2;

    //register the queues
    q_registry[q1.id] = &q1;
    q_registry[q2.id] = &q2;

    //connect the queues
    connect(&q1, &q2);

    //Generate first event in the first queue
    event_list.push(std::make_tuple(trunc_expon(mean_interarrival_time, a, b), A, q1.id));
}

void timing(void) {
    if (event_list.empty()) {
        std::cerr << "Event list empty at (simulated) time " << sim_clock;
        exit(EXIT_FAILURE);
    }

    tup_t next_event = event_list.top();
    event_list.pop();

    //Move (simulated) clock to the next event arrival
    sim_clock = std::get<0>(next_event);
    next_event_type = std::get<1>(next_event);
    next_queue_id = std::get<2>(next_event);
}

//schedule departure (= service completion) from q
void schedule_departure_event_from(q_info_t* q){
    //If next queue doesn't exist, the packet has been completely processed by the queue system
    if (!q->next){
        processed_pkts += 1;
    }

    float service_time = trunc_expon(q->mean_service_time, a, b);

    //Schedule departure from the queue
    event_list.push(std::make_tuple(sim_clock + service_time, D, q->id));
    total_service += service_time;
}

void arrival_event(q_info_t* q) {
    std::cout << "arrival   " << q->name << "  (sim. time: " << sim_clock << ") (queue size: " << q->n_pkts << ")" << std::endl;

    if (q->status == BUSY) { //Packet cannot be processed immediately, keep it pending
        q->n_pkts += 1;

        if (q->n_pkts > q_limit){
            std::cerr << q->name << " overflow at (simulated) time " << sim_clock << std::endl;
            std::cerr << "Config: mean service time: " << q->mean_service_time << "  ||  mean inter-arrival time: "<< mean_interarrival_time << std::endl;

            //Abort test and remove the output file (if it exits) to avoid ambiguities
            std::remove("output.txt");
            exit(EXIT_FAILURE); 
        }

        q->pending_pkts.push_back(sim_clock);
    }
    else { //Process packet immediately (no queue delay)
        q->status = BUSY;
        schedule_departure_event_from(q);
    }

    //schedule next arrival (or else the simulation ends), but only for the first queue
    if(q->id == 1){
        event_list.push(std::make_tuple(sim_clock + trunc_expon(mean_interarrival_time, a, b), A, q->id));
    }
}

void departure_event(q_info_t* q) {
    std::cout << "departure " << q->name << "  (sim. time: " << sim_clock << ") (queue size: " << q->n_pkts << ")" << std::endl;

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
    ostream << avg_system_delay << std::endl;

    ostream.close();
}


//NOTE: "mean" is not a rate, but a time
//NOTE: The original formula is -(ln(1-U)/lambda), but the -1 is negligible: since 1-U and U are both uniform
float expon(float mean){ 
    return -mean * log(lcgrand(seed));
}

float trunc_expon(float mean, float a, float b){
    return -mean * log(exp(-a/mean) - (exp(-a/mean) - exp(-b/mean)) * lcgrand(seed));
    // original impl: return a - (log(1 - lcgrand(seed) * (1 - exp((a - b) / mean)))) * mean;
}


void connect(q_info_t* q1, q_info_t* q2){
    if (q1->next){
        std::cerr << q1->name << " was already connected to a queue" << std::endl;
        exit(EXIT_FAILURE);
    }

    q1->next = q2;
}
