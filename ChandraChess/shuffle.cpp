#include <fstream>
#include <iostream>
#include <random>
#include <thread>
#define THREADS 8
void shuffleWorker(int thread, int* threadOffsets, unsigned long* lineOffsets, int* indices, volatile int* progress, volatile bool* finished) {
  std::ifstream dataset("data.txt");
  std::ofstream datasetShuffled("./shards/" + std::to_string(thread) + ".txt");
  std::string line;
  int start = threadOffsets[thread];
  int end = threadOffsets[thread + 1];
  for (int i = start; i < end; i++) {
    dataset.clear();
    dataset.seekg(lineOffsets[indices[i]]);
    std::getline(dataset, line);
    datasetShuffled << line << std::endl;
    progress[thread] = progress[thread] + 1;
  }
  dataset.close();
  datasetShuffled.close();
  finished[thread] = true;
}
int main() {
  int numberOfLines = 0;
  std::ifstream dataset("data.txt");
  std::string line;
  while (std::getline(dataset, line)) numberOfLines += 1;
  std::cout << "Calculated Number of Lines" << std::endl;
  unsigned long unsignedLongSize = sizeof(unsigned long);
  unsigned long intSize = sizeof(int);
  unsigned long numberOfLinesUnsignedLong = (unsigned long)numberOfLines;
  unsigned long* lineOffsets = (unsigned long*)malloc(numberOfLinesUnsignedLong * unsignedLongSize);
  int* indices = (int*)malloc(numberOfLinesUnsignedLong * unsignedLongSize);
  for (int i = 0; i < numberOfLines; i++) indices[i] = i;
  std::cout << "Set Indices" << std::endl;
  int lineOffsetIndex = 0;
  dataset.clear();
  dataset.seekg(0);
  lineOffsets[lineOffsetIndex] = 0ul;
  while (true) {
    if (!std::getline(dataset, line)) break;
    lineOffsets[++lineOffsetIndex] = (unsigned long)dataset.tellg();
  }
  std::cout << "Set Line Offsets" << std::endl;
  std::random_device rd; 
  std::mt19937 gen(rd());
  std::uniform_real_distribution<long double> d(0.0l, 1.0l);
  for (int i = numberOfLines - 1; i > 0; i--) {
    int j = int(round(d(gen) * (long double)(i)));
    int iEntry = indices[i];
    int jEntry = indices[j];
    indices[i] = jEntry;
    indices[j] = iEntry;
  }
  std::cout << "Generated Shuffled Indices" << std::endl;
  dataset.close();
  // Multithreaded random access
  int threadSizes[THREADS];
  int threadOffsets[THREADS + 1];
  int remainder = numberOfLines % THREADS;
  int quotient = numberOfLines / THREADS;
  for (int i = 0; i < THREADS; i++) threadSizes[i] = quotient;
  for (int i = 0; i < remainder; i++) threadSizes[i] += 1;
  threadOffsets[0] = 0;
  int sum = 0;
  for (int i = 1; i <= THREADS; i++) {
    sum += threadSizes[i - 1];
    threadOffsets[i] = sum;
  }
  std::thread workerThreads[THREADS];
  volatile bool finished[THREADS];
  volatile int progress[THREADS];
  for (int i = 0; i < THREADS; i++) {
    finished[i] = false;
    progress[i] = 0;
  }
  for (int i = 0; i < THREADS; i++) workerThreads[i] = std::thread(shuffleWorker, i, threadOffsets, lineOffsets, indices, progress, finished);
  int lastProgress = 0;
  int progressReportInterval = 10000;
  while (true) {
    int totalProgress = 0;
    for (int i = 0; i < THREADS; i++) totalProgress += progress[i];
    if (totalProgress - lastProgress > progressReportInterval) {
      std::cout << "Filled: " << totalProgress << std::endl;
      lastProgress = totalProgress;
    }
    bool shouldBreak = true;
    for (int i = 0; i < THREADS; i++) {
      if (!finished[i]) {
        shouldBreak = false;
        break;
      }
    }
    if (shouldBreak) break;
  }
  for (int i = 0; i < THREADS; i++) {
    if (workerThreads[i].joinable()) {
      workerThreads[i].join();
    }
  }
  return 0;
}
