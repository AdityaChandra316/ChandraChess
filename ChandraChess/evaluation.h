#pragma once
#include "state.h"
// Number of weights and biases
const static int numberOfFeatureTransformerWeights = 768 * ACCUMULATOR_SIZE;
const static int numberOfFeatureTransformerBiases = ACCUMULATOR_SIZE;
const static int combinedAccumulatorSize = 2 * ACCUMULATOR_SIZE;
const static int numberOfOutputLayerWeights = combinedAccumulatorSize;
const static int numberOfOutputLayerBiases = 1;
// Weights and biases
extern int featureTransformerWeightsWhite[768][ACCUMULATOR_SIZE];
extern int featureTransformerWeightsBlack[768][ACCUMULATOR_SIZE];
extern int featureTransformerBiasesWhite[numberOfFeatureTransformerBiases];
extern int featureTransformerBiasesBlack[numberOfFeatureTransformerBiases];
extern int outputLayerWeights[numberOfOutputLayerWeights];
extern int outputLayerBiases[numberOfOutputLayerBiases];
void loadNnue();
void initializeAccumulators(board& inputBoard);
void makeAccumulatorMove(board& inputBoard, int move);
void takeAccumulatorMove(board& inputBoard);
int evaluate(board& inputBoard);
