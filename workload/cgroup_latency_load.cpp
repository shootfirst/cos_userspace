// https://blog.csdn.net/CHNIM/article/details/130626388

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include <chrono> 
#include <ostream>
#include <iostream>
#include <random>
#include <vector>

#include "thread.h"
#include "cos_cgroup.h"


class Timer {
 public:
  Timer() { startTime_ = std::chrono::high_resolution_clock::now(); }
  ~Timer() {
    fprintf(stderr, "%lld\n", std::chrono::duration_cast<std::chrono::nanoseconds>(
                     std::chrono::high_resolution_clock::now() - startTime_)
                     .count() / 1000000ULL);
  }

 private:
  std::chrono::high_resolution_clock::time_point startTime_;
};
 
// 定义神经网络的结构
typedef struct {
    int input_size; // 输入层的大小
    int hidden_size; // 隐藏层的大小
    int output_size; // 输出层的大小
    double *W1; // 第一层的权重
    double *b1; // 第一层的偏置
    double *W2; // 第二层的权重
    double *b2; // 第二层的偏置
} NeuralNet;
 
double sigmoid(double x) {
    return 1 / (1 + exp(-x));
}
 
 
// 初始化权重和偏置
void init_net(NeuralNet *net) {
    net->W1 = (double*) malloc(net->input_size * net->hidden_size * sizeof(double));
    net->b1 = (double*) malloc(net->hidden_size * sizeof(double));
    net->W2 = (double*) malloc(net->hidden_size * net->output_size * sizeof(double));
    net->b2 = (double*) malloc(net->output_size * sizeof(double));
    srand(time(NULL));
    // 初始化为随机值
    for (int i = 0; i < net->input_size * net->hidden_size; i++) {
        net->W1[i] = (double)rand()/RAND_MAX;
    }
    for (int i = 0; i < net->hidden_size * net->output_size; i++) {
        net->W2[i] = (double)rand()/RAND_MAX;
    }
    for (int i = 0; i < net->hidden_size; i++) {
        net->b1[i] = (double)rand()/RAND_MAX;
    }
    for (int i = 0; i < net->output_size; i++) {
        net->b2[i] = (double)rand()/RAND_MAX;
    }
}
 
// 前向传播算法
void forward(NeuralNet *net, double *input, double *output) {
    double *h1 = (double*) malloc(net->hidden_size * sizeof(double));
    double *h2 = (double*) malloc(net->output_size * sizeof(double));
    // 第一层的计算
    for (int i = 0; i < net->hidden_size; i++) {
        h1[i] = 0;
        for(int j = 0; j < net->input_size; j++){
            h1[i] += input[j] * net->W1[j*net->hidden_size+i];
        }
        h1[i] += net->b1[i];
        h1[i] = tanh(h1[i]);
    }
    // 第二层的计算
    for (int i = 0; i < net->output_size; i++) {
        h2[i] = 0;
        for (int j = 0; j < net->hidden_size; j++) {
            h2[i] += h1[j] * net->W2[j*net->output_size+i];
        }
        h2[i] += net->b2[i];
    }
    // 输出层的激活函数
    for (int i = 0; i < net->output_size; i++) {
        output[i] = sigmoid(h2[i]);
    }
    free(h1);
    free(h2);
}
 
// 反向传播算法
void backward(NeuralNet *net, double *input, double *target, double learning_rate) {
    double *h1 = (double*) malloc(net->hidden_size * sizeof(double));
    double *h2 = (double*) malloc(net->output_size * sizeof(double));
    double *delta1 = (double*) malloc(net->hidden_size * sizeof(double));
    double *delta2 = (double*) malloc(net->output_size * sizeof(double));
    // 第一层的计算
    for (int i = 0; i < net->hidden_size; i++) {
        h1[i] = 0;
        for (int j = 0; j < net->input_size; j++) {
            h1[i] += input[j] * net->W1[j*net->hidden_size+i];
        }
        h1[i] += net->b1[i];
        h1[i] = tanh(h1[i]);
    }
    // 第二层的计算
    for (int i = 0; i < net->output_size; i++) {
        h2[i] = 0;
        for (int j = 0; j < net->hidden_size; j++) {
            h2[i] += h1[j] * net->W2[j*net->output_size+i];
        }
        h2[i] += net->b2[i];
    }
    // 输出层的激活函数
    for (int i = 0; i < net->output_size; i++) {
        h2[i] = sigmoid(h2[i]);
        delta2[i] = h2[i] * (1-h2[i]) * (target[i]-h2[i]);
    }
    // 第一层的误差传递
    for (int i = 0; i < net->hidden_size; i++) {
        delta1[i] = 0;
        for (int j = 0; j < net->output_size; j++) {
            delta1[i] += delta2[j] * net->W2[i*net->output_size+j];
        }
        delta1[i] *= (1-h1[i]) * (1+h1[i]);
    }
    // 权重和偏置的更新
    for (int i = 0; i < net->hidden_size; i++) {
        for (int j = 0; j < net->input_size; j++) {
            net->W1[j*net->hidden_size+i] += learning_rate * delta1[i] * input[j];
        }
        net->b1[i] += learning_rate * delta1[i];
    }
    for (int i = 0; i < net->output_size; i++) {
        for (int j = 0; j < net->hidden_size; j++) {
            net->W2[j*net->output_size+i] += learning_rate * delta2[i] * h1[j];
        }
        net->b2[i] += learning_rate * delta2[i];
    }
    free(h1);
    free(h2);
    free(delta1);
    free(delta2);
}
 
// 训练神经网络
void train(NeuralNet *net, double *inputs, double *targets, int num_epochs, int num_inputs, double learning_rate) {
    for (int i = 0; i < num_epochs; i++) {
        for (int j = 0; j < num_inputs; j++) {
            double *input = &inputs[j*net->input_size];
            double *target = &targets[j*net->output_size];
            backward(net, input, target, learning_rate);
        }
    }
}
 
// 使用神经网络进行预测
void predict(NeuralNet *net, double *input, double *output) {
    forward(net, input, output);
}

void do_calculate() {
    for (int i = 0; i < 1000; i ++) {
        NeuralNet net;
        net.input_size = 2;
        net.hidden_size = 3;
        net.output_size = 1;
        init_net(&net);

        std::random_device rd; // 使用硬件熵作为种子
        std::mt19937 gen(rd()); // 使用 Mersenne Twister 引擎
        std::uniform_real_distribution<double> dis(0.0, 2.0); // 生成 0 到 2 之间的均匀分布的随机数

        double inputs[] = {(int)dis(gen),(int)dis(gen),(int)dis(gen),(int)dis(gen),(int)dis(gen),(int)dis(gen),(int)dis(gen),(int)dis(gen)};
        double targets[] = {(int)dis(gen),(int)dis(gen),(int)dis(gen),(int)dis(gen)};
        train(&net, inputs, targets, 1000, 8, 0.1);
        double output[1];
        predict(&net, inputs, output);
    }
}

int main() {
    printf("cgroup load\n");
    auto coscg = new CosCgroup;
    coscg->adjust_rate(600);

    auto worker_thread = new CosThread([] {
        sleep(1);
        Timer t = Timer();
        do_calculate();
    });

    int interfering_thread_num = 40;
    std::vector<CosThread*> interfering_thread_vec(0);
    for (int i = 0; i < interfering_thread_num; i++) {

        interfering_thread_vec.push_back(
            new CosThread([&] {
                sleep(1);
                int tid = gettid();
                coscg->add_thread(tid);
                do_calculate();
            })
        );
    }

    worker_thread->join();
    for(auto& t : interfering_thread_vec)  t->join();

    return 0;
}
