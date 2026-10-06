#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/PerformanceMeasure.h>

PerformanceMeasure::PerformanceMeasure(const char* name, float r, float g, float b, bool* isActive) {
	Name = name;
	Colors.R = r;
	Colors.G = g;
	Colors.B = b;
	Active = isActive;
};

PerformanceMeasure::PerformanceMeasure(const char* name, float r, float g, float b) {
	Name = name;
	Colors.R = r;
	Colors.G = g;
	Colors.B = b;
	Active = nullptr;
};

PerformanceMeasure::PerformanceMeasure(const char* name) {
	Name = name;
	Colors.R = 0.0f;
	Colors.G = 1.0f;
	Colors.B = 0.0f;
	Active = nullptr;
};

bool PerformanceMeasure::IsActive() {
	if (Active != nullptr) {
		return *Active;
	}

	return true;
}

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
