#ifndef SHADOW_UTIL_PARSER_HPP
#define SHADOW_UTIL_PARSER_HPP

#include "shadow/network.hpp"

#include <string>

class Parser {
public:
  void ParseNetworkProtoTxt(Network *net, std::string prototxt_file, int batch);
  void LoadWeights(Network *net, std::string weight_file);

private:
  void ParseNet(Network *net);
  Layer *LayerFactory(shadow::LayerParameter layer_param, Blob<BType> *blob);

  void LoadWeightsUpto(Network *net, std::string weight_file, int cut_off);
};

#endif // SHADOW_UTIL_PARSER_HPP
