#pragma once

#include "quantforge/experiment/experiment.hpp"
#include "quantforge/sim/naive_bar_backtester.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace quantforge::report {

/// Portfolio-Analyser-style research report surfaces (markdown + CSV).
struct LeakageCaseStudy {
    std::string title{"naive_bar_vs_lob"};
    experiment::StrategyResult lob{};
    sim::NaiveBarResult naive{};
};

struct ResearchReport {
    experiment::ExperimentReport experiment{};
    std::vector<LeakageCaseStudy> leakage_cases{};
    std::string notes;
};

std::string formatMarkdown(const ResearchReport& report);
std::string formatCsv(const experiment::ExperimentReport& report);

void writeReportFiles(
    const ResearchReport& report,
    const std::filesystem::path& markdown_path,
    const std::filesystem::path& csv_path
);

} // namespace quantforge::report
