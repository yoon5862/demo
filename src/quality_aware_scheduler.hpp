#ifndef _QUALITY_AWARE_SCHEDULER_H
#define _QUALITY_AWARE_SCHEDULER_H

#define THRESHOLD_CPU 40 // Standard CPU utilization of WebOS board
#define THRESHOLD_MEMORY 90 // Strict Memory utilization of WebOS board
#define THRESHOLD_LLCMISS 90 // Frequently LLC Miss rate of WebOS board

struct nn_stats {
    bool        quant_state;  // TRUE: quantized model (INT8);  FALSE: none-quantized model (FP32)
    int       pruning_degree; // sparsity ratio of nn
    float       cpu_util;       // Consumed CPU core utilization; profiled value
    float       memory_util;    // Consumed memory utilization; profiled value
    float       model_size;     // KB
    double       latency;       // ms
};

int exec_mobilenet_v2();
std::string exec_demo();
void quality_aware_scheduler(char *model_name);
extern float util_cpu_, util_memory_, llc_miss_rate_;

#endif 