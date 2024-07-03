/*
**
** LolPlus ABR
**
 */
#define CONST_NUMBER_OF_BITRATES 5

#ifndef LolPlus_ALGORITHM_H
#define LolPlus_ALGORITHM_H

#include "tcp-stream-adaptation-algorithm.h"

namespace ns3 {

	/**
	 * \ingroup tcpStream
	 * \brief Implementation of LolPlus ABR
	 */
	class LolPlus : public AdaptationAlgorithm
	{
		public:

			LolPlus (  const videoData &videoData,
			const playbackData & playbackData,
			const bufferData & bufferData,
			const throughputData & throughput,
			int64_t chunks, int cmaf);

			algorithmReply GetNextRep (const int64_t segmentCounter, int64_t clientId);

		private:

			const int64_t m_highestRepIndex;
			int64_t chunks;
			int cmaf;

			int const AVERAGE_THROUGHPUT_SAMPLE_AMOUNT_LIVE = 3;
			int SAMPLE_SIZE = AVERAGE_THROUGHPUT_SAMPLE_AMOUNT_LIVE;
			double const THROUGHPUT_DECREASE_SCALE = 1.3;
			double const THROUGHPUT_INCREASE_SCALE = 1.3;

			const double DWS_TARGET_LATENCY = 1.5;
			const double DWS_BUFFER_MIN = 0.3;

			struct neuron {
				int qualityIndex;
				double bitrate;
				double state_throughput;
				double state_latency;
				double state_rebuffer;
				double state_switch;
			};
			std::vector<neuron> somBitrateNeurons;
			bool neuronsSetup = false;

			std::vector<std::vector<double>> sortedCenters;
			double weights[4];
			double previousLatency = 0;

		    const static int weightTypeCount = 4;
		    double weightOptions[(int)std::pow(5.0, weightTypeCount)][weightTypeCount];

	        double bitrateNormalizationFactor = 1;
	        double latencyNormalizationFactor = 100;

			struct latency_weight {
				double threshold;
				double penalty;
			};

			struct weights_struct {
				double bitrateReward;
				double bitrateSwitchPenalty;
				double rebufferPenalty;
				latency_weight latencyPenaltyLow;
				latency_weight latencyPenaltyHigh;
				double playbackSpeedPenalty;
			};

			struct qoe {
				weights_struct weights; 
				double bitrateWSum = 0;
				double bitrateSwitchWSum = 0;
				double rebufferWSum = 0;
				double latencyWSum = 0;
				double playbackSpeedWSum = 0;
				double totalQoe = 0;
				double lastBitrate = -1;
			};

			int lastSegmentQuality;

			double targetLatency = 1.5;
			double bufferMin = 0.3;
			double segmentDuration;

			// Declare functions here
			LolPlus::qoe createQoe(double segmentDuration, double maxBitrate, double minBitrate);
			void logMetricsInQoe(qoe &qoeInfo, double bitrate, double rebufferTime, double latency, double playbackSpeed);
			double getMaxThroughput();
			double getDistance(double a[], double b[], double w[]);
			void findWeightVector(double latency, double buffer, double rebuffer, double throughput, double playbackRate);
			void updateNeurons(int neuron, double throughput, double latency, double rebuffer, double s_switch);

	};

} // namespace ns3
#endif /* LolPlus_ALGORITHM_H */