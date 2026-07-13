#include "time-tracker.hpp"

#include <obs-frontend-api.h>

TimeTracker &TimeTracker::Instance()
{
	static TimeTracker instance;
	return instance;
}

void TimeTracker::HandleFrontendEvent(int eventValue)
{
	const auto event = static_cast<obs_frontend_event>(eventValue);
	const auto now = std::chrono::steady_clock::now();

	switch (event) {
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		recording_ = true;
		paused_ = false;
		recordStart_ = now;
		recordPausedAccum_ = std::chrono::milliseconds{0};
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		recording_ = false;
		paused_ = false;
		break;
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
		paused_ = true;
		pauseStart_ = now;
		break;
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		if (paused_)
			recordPausedAccum_ += std::chrono::duration_cast<std::chrono::milliseconds>(now - pauseStart_);
		paused_ = false;
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		streaming_ = true;
		streamStart_ = now;
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		streaming_ = false;
		break;
	default:
		break;
	}
}

void TimeTracker::SyncCurrentState()
{
	const auto now = std::chrono::steady_clock::now();

	// Best-effort only: if recording/streaming was already active before this
	// callback was registered (e.g. a hot reload), the true start time is
	// unknown, so elapsed time starts counting from "now" instead.
	if (obs_frontend_recording_active()) {
		recording_ = true;
		paused_ = obs_frontend_recording_paused();
		recordStart_ = now;
		recordPausedAccum_ = std::chrono::milliseconds{0};
	}
	if (obs_frontend_streaming_active()) {
		streaming_ = true;
		streamStart_ = now;
	}
}

TimeTracker::State TimeTracker::GetState() const
{
	State state;
	state.recording = recording_;
	state.recordingPaused = paused_;
	state.streaming = streaming_;

	if (recording_) {
		const auto endPoint = paused_ ? pauseStart_ : std::chrono::steady_clock::now();
		auto elapsed = (endPoint - recordStart_) - recordPausedAccum_;
		if (elapsed.count() < 0)
			elapsed = std::chrono::steady_clock::duration::zero();
		state.recordingElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
	}
	if (streaming_) {
		const auto elapsed = std::chrono::steady_clock::now() - streamStart_;
		state.streamingElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
	}

	return state;
}
