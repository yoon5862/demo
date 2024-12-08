#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <cmath>

#include "monitoring.hpp"
#include "util.hpp"
#include "quality_aware_scheduler.hpp"

/*
 * Update hardware utilization table of mobilenet-v2 depend on the utilization of CPU

 * IN:
 * @portfolio           NN Portfolio having CPU/Memory utilization, Latency, Model size
 * @util_cpu            Current CPU's utilization
 * @model               name of model
 * 
 * Out:
 * Updated NN Portfolio
*/
void update_utilization_table(std::vector<nn_stats> *portfolio, float util_cpu, std::string model_name)
{
    int memory_tlmkb = read_meminfo();
    int lat_key = util_cpu < THRESHOLD_CPU ? 0 : util_cpu;
    // std::cout << lat_key << std::endl;

    FILE *fp;
    char line[128], classify[10];
    nn_stats stats;
    std::string file_name = "portfolio/portfolio_";
    file_name += model_name + "_";
    file_name += std::to_string(lat_key) + ".csv";
    std::cout << file_name << std::endl;

    if ((fp = fopen(file_name.c_str(), "r")) == NULL) {
        std::cout << "[ERROR] Can not find NN model's portfolio" << std::endl;
        exit(1);
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        sscanf(line, "%*[^','], %d, %d, %f, %f, %f, %lf", 
            &(stats.quant_state), &stats.pruning_degree, &stats.cpu_util, &stats.memory_util, &stats.model_size, &stats.latency);
        portfolio->push_back(stats);
    }
    fclose(fp);
}

/*
 * Execution Mobilenet-v2 following scheduling policy
*/
int exec_mobilenet_v2()
{
    std::vector<nn_stats> mobilenet_v2;
    float util_cpu = std::round(util_cpu_/10)*10;
    update_utilization_table(&mobilenet_v2, util_cpu, "mobilenet-v2");

    std::string model_name = "mobilenet-v2";
    float COUNTER_CPU = 1 + (THRESHOLD_CPU + util_cpu)/100;
    double base_latency = 51.8; // hard cording

    int idx = 0;
    
    // When memory utilization is over threshold, we update counter_cpu for strict bar.
    if (util_memory_ > THRESHOLD_MEMORY)
        COUNTER_CPU -= util_memory_/100;

    std::cout << "COUNTER_CPU: " << COUNTER_CPU << std::endl;
    //// Find appropriate NN
    // When LLC miss rate and memory utilization is over threshold, select quantization model
    if (util_memory_ > THRESHOLD_MEMORY && llc_miss_rate_ > THRESHOLD_LLCMISS) {
        model_name += "_int8.tflite";
    }
    else {
        while (true) {
            // Select appropriate model to compare the its latency with base_model's latency * counter_cpu
            if (base_latency * COUNTER_CPU >= mobilenet_v2[idx].latency) {

                // Set model name
                if (mobilenet_v2[idx].quant_state)
                    model_name += "_int8";
                else
                    model_name += "_fp32";    
                if (mobilenet_v2[idx].pruning_degree != 0){
                    model_name += "_" + std::to_string(mobilenet_v2[idx].pruning_degree);
                }
                model_name += ".tflite";
                std::cout << model_name << std::endl;
                break;
            }

            // Next NN variant model
            idx++;

            // If every NN model in the NN portfolio did not pass the constricted latency threshold, update COUNTER_CPU
            if (idx == int(mobilenet_v2.size())) {
                idx = 0;
                COUNTER_CPU += 1; // specify
            }
        }
    }

    /*
    Execute selected NN model
    */
    std::string exec_operation_ = "benchmark_model --graph="; // TFLite execution file
    exec_operation_ += model_name; // model name
    exec_operation_ += " --num_runs=100 --num_threads=4 --use_xnnpack=true"; // Test option
    std::cout << "Model: mobilenet-v2; Data type: " << (mobilenet_v2[idx].quant_state ? "INT8" : "FP32") \
            << "; Pruning degree:" << mobilenet_v2[idx].pruning_degree << std::endl;
    system(exec_operation_.c_str());

    return 0;
}


/*
 * Execution DEMO model following scheduling policy
 * Return:
 * @model_name      demo_big.tflite, demo_middle_tflite, demo_small.tflite
*/
std::string exec_demo()
{
    std::vector<nn_stats> demo_model;
    float util_cpu = std::round(util_cpu_/10)*10;
    update_utilization_table(&demo_model, util_cpu, "demo");

    std::string model_name = "demo";
    float COUNTER_CPU = 1 + (THRESHOLD_CPU + util_cpu)/100;
    double base_latency = 1303.27; // hard cording

    int idx = 0;
    
    // When memory utilization is over threshold, we update counter_cpu for strict bar.
    if (util_memory_ > THRESHOLD_MEMORY)
        COUNTER_CPU -= util_memory_/100;

    std::cout << "COUNTER_CPU: " << COUNTER_CPU << std::endl;
    //// Find appropriate NN
    // When LLC miss rate and memory utilization is over threshold, select quantization model
    if (util_memory_ > THRESHOLD_MEMORY && llc_miss_rate_ > THRESHOLD_LLCMISS) {
        model_name += "_Nano.tflite";
    }
    else {
        while (true) {
            // Select appropriate model to compare the its latency with base_model's latency * counter_cpu
            if (base_latency * COUNTER_CPU >= demo_model[idx].latency) {

                // Set model name
                if (demo_model[idx].quant_state)
                    model_name += "_Nano";
                else {
                    if (demo_model[idx].pruning_degree != 0)
                        model_name += "_Mid";
                    else
                        model_name += "_Large";
                }
                model_name += ".tflite";
                std::cout << model_name << std::endl;
                break;
            }

            // Next NN variant model
            idx++;

            // If every NN model in the NN portfolio did not pass the constricted latency threshold, update COUNTER_CPU
            if (idx == int(demo_model.size())) {
                idx = 0;
                COUNTER_CPU += 1; // specify
            }
        }
    }

    /*
    Return selected NN model
    */
    return model_name;
}


void quality_aware_scheduler(char *model_name)
{
    if (strcmp(model_name, "mobilenet_v2") == 0)    {
        std::cout << "Run Mobilenet_v2" << std::endl;
        exec_mobilenet_v2();        
    }
    if (strcmp(model_name, "demo") == 0)    {
        std::cout << "Run DEMO_NN" << std::endl;
        exec_demo();        
    }
    else if (strcmp(model_name, "exit") == 0)    {
        std::cout << "[Finish] Quality-aware Scheduler" << std::endl;
        exit(1);
    }
    else {
        std::cout << "[ERROR] Plz enter the exact model name" << std::endl;;
        std::cout << "CPU util: " << util_cpu_ << "; Memory util: " << util_memory_ << "; LLC Miss Rate: " << llc_miss_rate_ << std::endl;
        // exit(1);
    }
}
