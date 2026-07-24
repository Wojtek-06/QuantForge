#pragma once

#include "quantforge/experiment/experiment.hpp"
#include "quantforge/experiment/walk_forward.hpp"
#include "quantforge/report/comparison_report.hpp"

#include <string>

namespace quantforge::report {

std::string formatExperimentJson(const experiment::ExperimentReport& report);
std::string formatWalkForwardJson(const experiment::WalkForwardReport& report);
std::string formatResearchJson(const ResearchReport& report);

} // namespace quantforge::report
