#define BATCH_SIZE 16384
#define ACCUMULATOR_SIZE 256
#define ALPHA 0.001
#define BETA_1 0.9
#define BETA_2 0.999
#define EPSILON 1e-8
#define THREADS 48
#define EPOCHS 10
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <math.h>
#include <map>
#include <random>
#include <string>
#include <thread>
// Maybe out of bounds input?
struct inputTensor {
  int activeIndices[2][32];
  int numberOfActiveIndices[2] = {0};
};
class DataGenerator {
  private:
    std::ifstream dataset;
    std::map<char, std::array<int, 2>> pieceToInteger;
  public:
    DataGenerator(std::string filePath) {
      dataset.open(filePath);
      pieceToInteger['K'] = {0, 6};
      pieceToInteger['Q'] = {1, 7};
      pieceToInteger['R'] = {2, 8};
      pieceToInteger['B'] = {3, 9};
      pieceToInteger['N'] = {4, 10};
      pieceToInteger['P'] = {5, 11};
      pieceToInteger['k'] = {6, 0};
      pieceToInteger['q'] = {7, 1};
      pieceToInteger['r'] = {8, 2};
      pieceToInteger['b'] = {9, 3};
      pieceToInteger['n'] = {10, 4};
      pieceToInteger['p'] = {11, 5};
    }
    void getItem(int* sidesToPlay, inputTensor* inputTensors, double* outputTensor) {
      for (int i = 0; i < BATCH_SIZE; i++) {
        std::string line;
        // Go back to beginning of dataset if we reached end
        if (!std::getline(dataset, line)) {
          dataset.clear();
          dataset.seekg(0);
          std::getline(dataset, line);
        }
        std::string position = line.substr(0, line.size() - 11);
        int sideToPlay = line[line.size() - 10] == 'w';
        double result = std::stod(line.substr(line.size() - 8, 8));
        sidesToPlay[i] = sideToPlay;
        outputTensor[i] = result;
        int square = 0;
        for (int j = 0; j < position.size(); j++) {
          char character = position[j];
          if (character == '/') continue;
          if (isdigit(character)) {
            square += (character - '0');
            continue;
          }
          for (int perspective = 0; perspective < 2; perspective++) {
            int pieceSquare = perspective == 1 ? square : square ^ 63;
            int nnueInputIndex = 64 * pieceToInteger[character][perspective] + pieceSquare;
            inputTensors[i].activeIndices[perspective][inputTensors[i].numberOfActiveIndices[perspective]++] = nnueInputIndex;
          }
          square++;
        }
      }
    }
};
class NNUE {
  private:
    // Width: N_Input 
    // Height: N_Output
    double t = 0.0;
    // Number of weights and biases
    const static int numberOfFeatureTransformerWeights = 768 * ACCUMULATOR_SIZE;
    const static int numberOfFeatureTransformerBiases = ACCUMULATOR_SIZE;
    const static int numberOfOutputLayerWeights = 2 * ACCUMULATOR_SIZE;
    const static int numberOfOutputLayerBiases = 1;
    const static int doubleSize = sizeof(double);
    const static int doublePointerSize = sizeof(double*);
    // Number of deltas
    const static int deltaMultiplier = doubleSize * THREADS;
    const static int deltaFeatureTransformerWeighsSize = numberOfFeatureTransformerWeights * deltaMultiplier;
    const static int deltaFeatureTransformerBiasesSize = numberOfFeatureTransformerBiases * deltaMultiplier;
    const static int deltaOutputLayerWeightsSize = numberOfOutputLayerWeights * deltaMultiplier;
    const static int deltaOutputLayerBiasesSize = numberOfOutputLayerBiases * deltaMultiplier;
    // Weight allocations
    double* featureTransformerWeightsWhite = (double*)calloc(numberOfFeatureTransformerWeights, doubleSize);
    double* featureTransformerWeightsBlack = (double*)calloc(numberOfFeatureTransformerWeights, doubleSize);
    double* featureTransformerBiasesWhite = (double*)calloc(numberOfFeatureTransformerBiases, doubleSize);
    double* featureTransformerBiasesBlack = (double*)calloc(numberOfFeatureTransformerBiases, doubleSize);
    double* outputLayerWeights = (double*)calloc(numberOfOutputLayerWeights, doubleSize);
    double* outputLayerBiases = (double*)calloc(numberOfOutputLayerBiases, doubleSize);
    double* weightsPointers[6] = {
      featureTransformerWeightsWhite, featureTransformerWeightsBlack, featureTransformerBiasesWhite, featureTransformerBiasesBlack, 
      outputLayerWeights, outputLayerBiases
    };
    int parameterCounts[6] = {
      numberOfFeatureTransformerWeights, numberOfFeatureTransformerWeights, numberOfFeatureTransformerBiases, numberOfFeatureTransformerBiases,
      numberOfOutputLayerWeights, numberOfOutputLayerBiases
    };
    // Delta weight allocations
    double* deltaFeatureTransformerWeightsWhite = (double*)malloc(deltaFeatureTransformerWeighsSize);
    double* deltaFeatureTransformerWeightsBlack = (double*)malloc(deltaFeatureTransformerWeighsSize);
    double* deltaFeatureTransformerBiasesWhite = (double*)malloc(deltaFeatureTransformerBiasesSize);
    double* deltaFeatureTransformerBiasesBlack = (double*)malloc(deltaFeatureTransformerBiasesSize);
    double* deltaOutputLayerWeights = (double*)malloc(deltaOutputLayerWeightsSize);
    double* deltaOutputLayerBiases = (double*)malloc(deltaOutputLayerBiasesSize);
    double* deltaPointers[6] = {
      deltaFeatureTransformerWeightsWhite, deltaFeatureTransformerWeightsBlack, deltaFeatureTransformerBiasesWhite, deltaFeatureTransformerBiasesBlack, 
      deltaOutputLayerWeights, deltaOutputLayerBiases
    };
    int offsets[6][THREADS];
    // m Allocations
    double* mFeatureTransformerWeightsWhite = (double*)calloc(numberOfFeatureTransformerWeights, doubleSize);
    double* mFeatureTransformerWeightsBlack = (double*)calloc(numberOfFeatureTransformerWeights, doubleSize);
    double* mFeatureTransformerBiasesWhite = (double*)calloc(numberOfFeatureTransformerBiases, doubleSize);
    double* mFeatureTransformerBiasesBlack = (double*)calloc(numberOfFeatureTransformerBiases, doubleSize);
    double* mOutputLayerWeights = (double*)calloc(numberOfOutputLayerWeights, doubleSize);
    double* mOutputLayerBiases = (double*)calloc(numberOfOutputLayerBiases, doubleSize);
    double* mPointers[6] = {
      mFeatureTransformerWeightsWhite, mFeatureTransformerWeightsBlack, mFeatureTransformerBiasesWhite, mFeatureTransformerBiasesBlack,
      mOutputLayerWeights, mOutputLayerBiases
    };
    // v Allocations
    double* vFeatureTransformerWeightsWhite = (double*)calloc(numberOfFeatureTransformerWeights, doubleSize);
    double* vFeatureTransformerWeightsBlack = (double*)calloc(numberOfFeatureTransformerWeights, doubleSize);
    double* vFeatureTransformerBiasesWhite = (double*)calloc(numberOfFeatureTransformerBiases, doubleSize);
    double* vFeatureTransformerBiasesBlack = (double*)calloc(numberOfFeatureTransformerBiases, doubleSize);
    double* vOutputLayerWeights = (double*)calloc(numberOfOutputLayerWeights, doubleSize);
    double* vOutputLayerBiases = (double*)calloc(numberOfOutputLayerBiases, doubleSize);
    double* vPointers[6] = {
      vFeatureTransformerWeightsWhite, vFeatureTransformerWeightsBlack, vFeatureTransformerBiasesWhite, vFeatureTransformerBiasesBlack,
      vOutputLayerWeights, vOutputLayerBiases
    };
    // Variables used in threading
    double meanSquaredErrors[THREADS] = {0.0};
    int batchOffsets[THREADS + 1] = {0};
    int parameterOffsets[THREADS + 1] = {0};
    double** weightsList;
    double** deltaList;
    double** mList;
    double** vList;
    // Data pointers
    int* sidesToPlay;
    inputTensor* inputTensors;
    double* outputTensor;
    std::thread workerThreads[THREADS];
    void featureTransformerForward(double* weights, double* biases, int* activeIndices, int numberOfActiveIndices, double* output) {
      for (int i = 0; i < ACCUMULATOR_SIZE; i++) {
        output[i] = biases[i];
        for (int j = 0; j < numberOfActiveIndices; j++) output[i] += weights[768 * i + activeIndices[j]];
      }
    }
    void featureTransformerBackward(double* outputGradients, int* activeIndices, int numberOfActiveIndices, double* weightGradients, double* biasGradients, int thread) {
      int weightGradientsStart = numberOfFeatureTransformerWeights * thread;
      int biasGradientsStart = ACCUMULATOR_SIZE * thread;
      for (int i = 0; i < ACCUMULATOR_SIZE; i++) {
        biasGradients[biasGradientsStart + i] += outputGradients[i];
        for (int j = 0; j < numberOfActiveIndices; j++) weightGradients[weightGradientsStart + 768 * i + activeIndices[j]] += outputGradients[i];
      }
    }
    void linearForward(double* weights, double* biases, double* input, int numberOfInputs, int numberOfOutputs, double* output) {
      for (int i = 0; i < numberOfOutputs; i++) {
        output[i] = biases[i];
        for (int j = 0; j < numberOfInputs; j++) output[i] += weights[numberOfInputs * i + j] * input[j];
      }
    }
    void linearBackward(double* outputGradients, double* input, int numberOfInputs, int numberOfOutputs, double* weightGradients, double* biasGradients, int thread) {
      int weightGradientsStart = numberOfInputs * numberOfOutputs * thread;
      int biasGradientsStart = numberOfOutputs * thread;
      for (int i = 0; i < numberOfOutputs; i++) {
        biasGradients[biasGradientsStart + i] += outputGradients[i];
        for (int j = 0; j < numberOfInputs; j++) weightGradients[weightGradientsStart + numberOfInputs * i + j] += input[j] * outputGradients[i];
      }
    }
    void calculateInputGradients(double* weights, int numberOfOutputs, double* outputGradients, void (*gradientFunction)(int, double*, double*), double* input, int numberOfInputs, double* inputGradients) {
      double inputNeuronActivationGradients[numberOfInputs];
      gradientFunction(numberOfInputs, input, inputNeuronActivationGradients);
      for (int i = 0; i < numberOfOutputs; i++) {
        for (int j = 0; j < numberOfInputs; j++) inputGradients[j] += inputNeuronActivationGradients[j] * weights[numberOfInputs * i + j] * outputGradients[i];
      }
    }
    static void relu(int layerSize, double* layer, double* activated) {
      for (int i = 0; i < layerSize; i++) activated[i] = std::max(0.0, layer[i]);
    }
    static void sigmoid(int layerSize, double* layer, double* activated) {
      for (int i = 0; i < layerSize; i++) activated[i] = 1.0 / (1.0 + std::exp(-layer[i]));
    }
    static void reluGradient(int layerSize, double* layer, double* gradients) {
      for (int i = 0; i < layerSize; i++) gradients[i] = layer[i] <= 0.0 ? 0.0 : 1.0;
    }
    static void sigmoidGradient(int layerSize, double* layer, double* gradients) {
      for (int i = 0; i < layerSize; i++) {
        double sigmoid = 1.0 / (1.0 + std::exp(-layer[i]));
        gradients[i] = sigmoid * (1.0 - sigmoid);
      }
    }
    void squaredErrorGradient(double* predicted, double* target, int numberOfOutputs, double* gradients) {
      for (int i = 0; i < numberOfOutputs; i++) gradients[i] = 2.0 * (predicted[i] - target[i]) / numberOfOutputs;
    }
    void gradientWorker(int thread) {
      double totalMeanSquaredError = 0.0;
      int startIndex = batchOffsets[thread];
      int endIndex = batchOffsets[thread + 1];
      // Resetting threaded weight gradients
      for (int parameter = 0; parameter < 6; parameter++) {
        int parameterCount = parameterCounts[parameter];
        int deltaListOffset = thread * parameterCount;
        double* deltaPointer = deltaPointers[parameter] + deltaListOffset;
        memset(deltaPointer, 0, parameterCount * doubleSize);
      }
      for (int i = startIndex; i < endIndex; i++) {
        // Forward
        double whiteAccumulator[ACCUMULATOR_SIZE] = {0.0};
        double blackAccumulator[ACCUMULATOR_SIZE] = {0.0};
        featureTransformerForward(featureTransformerWeightsWhite, featureTransformerBiasesWhite, inputTensors[i].activeIndices[1], inputTensors[i].numberOfActiveIndices[1], whiteAccumulator);
        featureTransformerForward(featureTransformerWeightsBlack, featureTransformerBiasesBlack, inputTensors[i].activeIndices[0], inputTensors[i].numberOfActiveIndices[0], blackAccumulator);
        double accumulator[2 * ACCUMULATOR_SIZE];
        double accumulatorActivated[2 * ACCUMULATOR_SIZE];
        int sideToPlay = sidesToPlay[i];
        if (sideToPlay == 1) {
          for (int j = 0; j < ACCUMULATOR_SIZE; j++) {
            accumulator[j] = whiteAccumulator[j];
            accumulator[j + ACCUMULATOR_SIZE] = blackAccumulator[j];
          }
        } else {
          for (int j = 0; j < ACCUMULATOR_SIZE; j++) {
            accumulator[j] = blackAccumulator[j];
            accumulator[j + ACCUMULATOR_SIZE] = whiteAccumulator[j];
          }
        }
        relu(2 * ACCUMULATOR_SIZE, accumulator, accumulatorActivated);
        double outputLayer[1] = {0.0};
        double outputLayerActivated[1];
        linearForward(outputLayerWeights, outputLayerBiases, accumulatorActivated, 2 * ACCUMULATOR_SIZE, 1, outputLayer);
        sigmoid(1, outputLayer, outputLayerActivated);
        // Backward
        double errorGradient[1];
        double outputGradient[1];
        double target[1] = {outputTensor[i]};
        // NNUE Error
        totalMeanSquaredError += std::pow(target[0] - outputLayerActivated[0], 2.0);
        squaredErrorGradient(outputLayerActivated, target, 1, errorGradient);
        sigmoidGradient(1, outputLayer, outputGradient);
        double outputByError[1] = {errorGradient[0] * outputGradient[0]};
        // Output
        linearBackward(outputByError, accumulatorActivated, 2 * ACCUMULATOR_SIZE, 1, deltaOutputLayerWeights, deltaOutputLayerBiases, thread);
        // Accumulator
        double accumulatorGradients[2 * ACCUMULATOR_SIZE] = {0.0};
        calculateInputGradients(outputLayerWeights, 1, outputByError, reluGradient, accumulator, 2 * ACCUMULATOR_SIZE, accumulatorGradients);
        double whiteAccumulatorGradients[ACCUMULATOR_SIZE];
        double blackAccumulatorGradients[ACCUMULATOR_SIZE];
        if (sideToPlay == 1) {
          for (int j = 0; j < ACCUMULATOR_SIZE; j++) {
            whiteAccumulatorGradients[j] = accumulatorGradients[j];
            blackAccumulatorGradients[j] = accumulatorGradients[j + ACCUMULATOR_SIZE];
          }
        } else {
          for (int j = 0; j < ACCUMULATOR_SIZE; j++) {
            blackAccumulatorGradients[j] = accumulatorGradients[j];
            whiteAccumulatorGradients[j] = accumulatorGradients[j + ACCUMULATOR_SIZE];
          }
        }
        featureTransformerBackward(whiteAccumulatorGradients, inputTensors[i].activeIndices[1], inputTensors[i].numberOfActiveIndices[1], deltaFeatureTransformerWeightsWhite, deltaFeatureTransformerBiasesWhite, thread);
        featureTransformerBackward(blackAccumulatorGradients, inputTensors[i].activeIndices[0], inputTensors[i].numberOfActiveIndices[0], deltaFeatureTransformerWeightsBlack, deltaFeatureTransformerBiasesBlack, thread);
      }   
      meanSquaredErrors[thread] = totalMeanSquaredError / BATCH_SIZE;
    }
    void updateWorker(int thread) {
      int startIndex = parameterOffsets[thread];
      int endIndex = parameterOffsets[thread + 1];
      for (int i = startIndex; i < endIndex; i++) {
        double g = 0.0;
        int deltaListOffset = THREADS * i;
        for (int j = 0; j < THREADS; j++) g += *deltaList[deltaListOffset + j];
        g /= BATCH_SIZE;
        *mList[i] = *mList[i] * BETA_1 + (1.0 - BETA_1) * g;
        *vList[i] = *vList[i] * BETA_2 + (1.0 - BETA_2) * g * g;
        double mhat = *mList[i] / (1.0 - std::pow(BETA_1, t + 1.0));
        double vhat = *vList[i] / (1.0 - std::pow(BETA_2, t + 1.0));
        *weightsList[i] -= ALPHA * mhat / (std::sqrt(vhat) + EPSILON);
      }
    }
  public:
    void initializeWeights() {
      double outputDistributionRange = 1.0 / std::sqrt(2.0 * ACCUMULATOR_SIZE);
      std::random_device rd; 
      std::mt19937 gen(rd());
      std::uniform_real_distribution<double> featureTransformerDistribution(0.0, std::sqrt(2.0 / 768.0)); 
      std::uniform_real_distribution<double> outputDistribution(-outputDistributionRange, outputDistributionRange); 
      for (int i = 0; i < numberOfFeatureTransformerWeights; i++) {
        featureTransformerWeightsWhite[i] = featureTransformerDistribution(gen);
        featureTransformerWeightsBlack[i] = featureTransformerDistribution(gen);
      }
      for (int i = 0; i < numberOfOutputLayerWeights; i++) outputLayerWeights[i] = outputDistribution(gen);
    }
    void initialize() {
      // Calculate how many positions each thread needs to backpropogate for
      int batchSizes[THREADS];
      int batchSizeRemainder = BATCH_SIZE % THREADS;
      int batchSizeQuotient = BATCH_SIZE / THREADS;
      for (int i = 0; i < THREADS; i++) batchSizes[i] = batchSizeQuotient;
      for (int i = 0; i < batchSizeRemainder; i++) batchSizes[i] += 1;
      int batchSum = 0;
      for (int i = 1; i <= THREADS; i++) {
        batchSum += batchSizes[i - 1];
        batchOffsets[i] = batchSum;
      }
      // Initialize offsets
      for (int parameter = 0; parameter < 6; parameter++) {
        for (int i = 0; i < THREADS; i++) offsets[parameter][i] = parameterCounts[parameter] * i;
      }
      // Combine all the parameters into a single list
      int numberOfParameters = 0;
      for (int parameter = 0; parameter < 6; parameter++) numberOfParameters += parameterCounts[parameter];
      int parameterListSize = numberOfParameters * doublePointerSize;
      weightsList = (double**)malloc(parameterListSize);
      deltaList = (double**)malloc(parameterListSize * THREADS);
      mList = (double**)malloc(parameterListSize);
      vList = (double**)malloc(parameterListSize);
      int parameterListIndex = 0;
      for (int parameter = 0; parameter < 6; parameter++) {
        for (int i = 0; i < parameterCounts[parameter]; i++) {
          weightsList[parameterListIndex] = &weightsPointers[parameter][i];
          int deltaListOffset = THREADS * parameterListIndex;
          for (int j = 0; j < THREADS; j++) deltaList[deltaListOffset + j] = &deltaPointers[parameter][offsets[parameter][j] + i];
          mList[parameterListIndex] = &mPointers[parameter][i];
          vList[parameterListIndex] = &vPointers[parameter][i];
          parameterListIndex++;
        }
      }
      // Calculate how many parameters each thread needs to update
      int parameterSizes[THREADS];
      int parameterSizeRemainder = numberOfParameters % THREADS;
      int parameterSizeQuotient = numberOfParameters / THREADS;
      for (int i = 0; i < THREADS; i++) parameterSizes[i] = parameterSizeQuotient;
      for (int i = 0; i < parameterSizeRemainder; i++) parameterSizes[i] += 1;
      int parameterSum = 0;
      for (int i = 1; i <= THREADS; i++) {
        parameterSum += parameterSizes[i - 1];
        parameterOffsets[i] = parameterSum;
      }
    }
    void loadParameters(std::string inputPath) {
      std::ifstream input(inputPath);
      std::string word;
      for (int parameter = 0; parameter < 6; parameter++) {
        for (int i = 0; i < parameterCounts[parameter]; i++) {
          input >> word;
          weightsPointers[parameter][i] = std::stod(word);
        }
      }
    }
    void setParameters(int* sidesToPlay, inputTensor* inputTensors, double* outputTensor) {
      this->sidesToPlay = sidesToPlay;
      this->inputTensors = inputTensors;
      this->outputTensor = outputTensor;
    }
    double backpropogate() {
      // Multithreaded gradient calculation and update
      for (int i = 0; i < THREADS; i++) workerThreads[i] = std::thread(&NNUE::gradientWorker, this, i);
      for (int i = 0; i < THREADS; i++) if (workerThreads[i].joinable()) workerThreads[i].join();
      for (int i = 0; i < THREADS; i++) workerThreads[i] = std::thread(&NNUE::updateWorker, this, i);
      for (int i = 0; i < THREADS; i++) if (workerThreads[i].joinable()) workerThreads[i].join();
      double meanSquaredError = 0.0;
      for (int i = 0; i < THREADS; i++) meanSquaredError += meanSquaredErrors[i];
      t += 1.0;
      return meanSquaredError;
    }
    void saveParameters(std::string outputPath) {
      std::ofstream outputFile(outputPath);
      for (int parameter = 0; parameter < 6; parameter++) {
        for (int i = 0; i < parameterCounts[parameter]; i++) {
          outputFile << weightsPointers[parameter][i] << " ";
        }
      }
    }
};
int main() {
  // Calculate number of lines
  int numberOfLines = 0;
  std::ifstream dataset("data.txt");
  std::string line;
  while (std::getline(dataset, line)) numberOfLines += 1;
  dataset.close();
  std::cout << "Calculated number of lines!" << std::endl;
  // Begin training
  int batchesPerEpoch = numberOfLines / BATCH_SIZE;
  DataGenerator dataGenerator("data.txt");
  NNUE nnue;
  nnue.initialize();
  nnue.initializeWeights();
  for (int epoch = 1; epoch <= EPOCHS; epoch++) {
    double tmse = 0.0;
    double n = 0.0;
    for (int batch = 1; batch <= batchesPerEpoch; batch++) {
      int sidesToPlay[BATCH_SIZE];
      inputTensor inputTensors[BATCH_SIZE];
      double outputTensor[BATCH_SIZE];
      dataGenerator.getItem(sidesToPlay, inputTensors, outputTensor);
      nnue.setParameters(sidesToPlay, inputTensors, outputTensor);
      double meanSquaredError = nnue.backpropogate();
      tmse += meanSquaredError;
      n += 1.0;
      std::cout << "Epoch Average: " << tmse / n << " Epoch: " << epoch << "/" << EPOCHS << " Batch: " << batch << "/" << batchesPerEpoch << std::endl;
    }
  }
  nnue.saveParameters("nnue.txt");
  return 0;
}
