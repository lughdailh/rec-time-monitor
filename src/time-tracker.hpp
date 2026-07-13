#pragma once

#include <chrono>
#include <cstdint>

class TimeTracker {
public:
	struct State {
		bool recording = false;
		bool recordingPaused = false;
		bool streaming = false;
		int64_t recordingElapsedMs = 0;
		int64_t streamingElapsedMs = 0;
	};

	static TimeTracker &Instance();

	// eventValue is an obs_frontend_event value, passed as int to avoid
	// pulling obs-frontend-api.h into this header.
	void HandleFrontendEvent(int eventValue);
	void SyncCurrentState();
	State GetState() const;

private:
	TimeTracker() = default;

	bool recording_ = false;
	bool paused_ = false;
	bool streaming_ = false;

	std::chrono::steady_clock::time_point recordStart_;
	std::chrono::steady_clock::time_point pauseStart_;
	std::chrono::milliseconds recordPausedAccum_{0};
	std::chrono::steady_clock::time_point streamStart_;
};
