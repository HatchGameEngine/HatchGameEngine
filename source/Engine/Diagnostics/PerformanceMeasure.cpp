#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/PerformanceMeasure.h>

void PerformanceMeasure::Reset() {
	Time = 0.0;
}

void PerformanceMeasure::Begin() {
	StartTime = Clock::GetTicks();
}

void PerformanceMeasure::End() {
	EndTime = Clock::GetTicks();

	Time = EndTime - StartTime;
}

void PerformanceMeasure::Accumulate() {
	EndTime = Clock::GetTicks();

	Time += EndTime - StartTime;
}
