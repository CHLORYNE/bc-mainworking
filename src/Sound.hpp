#ifndef __SOUND_HPP_INCLUDED__
#define __SOUND_HPP_INCLUDED__

#define FRAMES_PER_BUFFER   (512)

#ifdef WITH_SOUND
#include <sndfile.h>
#include <portaudio.h>
#endif // WITH_SOUND
#include <string.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

class Sound
{
public:

	Sound();
	~Sound();
	//KYARA COLLISION AND PROXY 
//KYARA COLLISION AND PROXY + INSIDE/OUTSIDE
	void load(std::string engineSoundFile,
		std::string waveSoundFile, std::string hornSoundFile,
		std::string alarmSoundFile, std::string proxyAlarmSoundFile,
		std::string collisionSoundFile, std::string insideSoundFile,
		std::string outsideSoundFile, std::string seagullSoundFile,
		std::string contactSoundFile, std::string radarAlarmSoundFile,
		std::string rainSoundFile, std::string stormSoundFile,
		std::string thunderSoundFile, std::string thunderboltSoundFile,
		std::string fireBurningSoundFile, std::string waterSoundFile, std::string fireAlarmSoundFile,
		std::string abandonAlarmSoundFile, std::string explosionSoundFile, std::string groanSoundFile,
		std::string steamSoundFile, std::string vhfSoundFile); // FIRE FEATURE
	void StartSound();
	void setVolumeWave(float vol);
	void setVolumeEngine(float vol);
	void setVolumeHorn(float vol);
	void setVolumeAlarm(float vol);
	float getVolumeWave() const;
	float getVolumeEngine() const;
	float getVolumeHorn() const;
	float getVolumeAlarm() const;
	//KYARA ENGINE PITCH: dynamic pitch-shift for engine_sound.wav, driven by throttle/RPM.
	//pitch is a playback-rate multiplier: 1.0 = recorded pitch, >1.0 = higher/faster (revving up),
	//<1.0 = lower/slower. Clamped in the setter to a sane audible range.
	void setPitchEngine(float pitch);
	float getPitchEngine() const;
	//KYARA COLLISION AND PROXY 
	void setVolumeProxyAlarm(float vol);
	void setVolumeCollision(float vol);
	//kyara outside inside volume
	void setVolumeInside(float vol);
	void setVolumeOutside(float vol);
	float getVolumeProxyAlarm() const;

	//kyara: independent radar guard-zone alarm channel (loops like proxy, own volume)
	void setVolumeRadarAlarm(float vol);
	float getVolumeRadarAlarm() const;
	float getVolumeCollision() const;
	//KYARA: fire the collision sound as a single one-shot (plays once, no looping)
	void triggerCollision();
	//KYARA SEAGULL: one-shot sound triggered by a key press (e.g. 'G'), same playback
	//model as the collision sound - plays through once, no looping.
	void setVolumeSeagull(float vol);
	float getVolumeSeagull() const;
	void triggerSeagull();
	// FIRE FEATURE
	void  setVolumeFireBurning(float vol); float getVolumeFireBurning() const;
	void  setVolumeWater(float vol);       float getVolumeWater() const;
	void  setVolumeFireAlarm(float vol);   float getVolumeFireAlarm() const;
	void  triggerFireAlarm();
	void  triggerExplosion();
	//KYARA SLAM: one sample for the hull coming down on the water, loaded separately from load()
	//so the (already very long) load() signature doesn't have to change. Every trigger carries
	//its own gain and playback rate, so one file covers a gentle landing and a hard one and never
	//sounds twice the same.
	void loadSlamSound(std::string slamFile);
	void setVolumeSlam(float vol);
	float getVolumeSlam() const;
	//gain 0..1 for this one impact; pitch = playback rate (1.0 = as recorded, <1 = deeper and
	//slower, which is what a heavy landing sounds like).
	void triggerSlam(float gain, float pitch);
	bool hasSlamSound() const;
	//Radar CPA/TCPA ("dangerous target") alarm: its own looping sound, distinct from the guard
	//zone alarm, and started from the beginning of its pattern each time it sounds.
	void loadCpaAlarmSound(std::string cpaAlarmFile);
	void setVolumeCpaAlarm(float vol);
	//Echo sounder shallow-water alarm: its own beeper, looping while it sounds
	void loadDepthAlarmSound(std::string depthAlarmFile);
	void setVolumeDepthAlarm(float vol);

	void  setVolumeVhf(float vol); float getVolumeVhf() const;
	void  setVolumeHelo(float vol); float getVolumeHelo() const;
	void  setVolumeGroan(float vol); float getVolumeGroan() const;
	void  setVolumeSteam(float vol); float getVolumeSteam() const;
	void triggerThunderbolt();   // NAUTITECH: one-shot hero-bolt clap
	//abandon ship alarm
	void setVolumeAbandonAlarm(float v);
	float getVolumeAbandonAlarm() const;
	//KYARA CONTACT: metal-on-quay one-shot, fired when the hull touches a land object.
//Same playback model as the collision one-shot: plays through once, never loops.
	void setVolumeContact(float vol);
	float getVolumeContact() const;
	void triggerContact();
	// KYARA WEATHER SFX
	void  setVolumeRain(float vol);    float getVolumeRain() const;    // looping, intensity-driven
	void  setVolumeStorm(float vol);   float getVolumeStorm() const;   // looping storm ambience
	void  setVolumeThunder(float vol); float getVolumeThunder() const;
	double getThunderPositionSeconds() const;   // KYARA: playback pos in the loop, for synced lightning

#ifdef WITH_SOUND
private:

	typedef struct
	{
		SNDFILE* fileWave;
		SNDFILE* fileEngine;
		SNDFILE* fileHorn;
		SNDFILE* fileAlarm;
		//SOUND OUTSIDE AND INSIDE BOAT KYARA
		SNDFILE* fileInside;
		SNDFILE* fileOutside;
		//abandon ship
		SNDFILE* fileAbandonAlarm;
		SF_INFO infoAbandonAlarm;
		//KYARA SOUND SYSTEM PROXY AND COLLISION
		SNDFILE* fileProxy;
		SNDFILE* fileRadarAlarm;
		SNDFILE* fileCollision;
		//KYARA SEAGULL
		SNDFILE* fileSeagull;
		SNDFILE* fileThunderbolt;
		//KYARA CONTACT
		SNDFILE* fileContact;
		//storm 
		SNDFILE* fileRain;    SF_INFO infoRain;
		SNDFILE* fileStorm;   SF_INFO infoStorm;
		SNDFILE* fileThunder; SF_INFO infoThunder;
		//FIRE FEATURE
		SNDFILE* fileFireBurning; SF_INFO infoFireBurning;
		SNDFILE* fileWater;       SF_INFO infoWater;
		SNDFILE* fileFireAlarm;   SF_INFO infoFireAlarm;
		SNDFILE* fileExplosion;   SF_INFO infoExplosion;
		SNDFILE* fileGroan;       SF_INFO infoGroan;
		SNDFILE* fileSteam;       SF_INFO infoSteam;
		SNDFILE* fileVhf;         SF_INFO infoVhf;
		SNDFILE* fileHelo;        SF_INFO infoHelo;
		//KYARA SLAM
		SNDFILE* fileSlam;        SF_INFO infoSlam;
		SNDFILE* fileCpaAlarm;    SF_INFO infoCpaAlarm;
		SNDFILE* fileDepthAlarm;  SF_INFO infoDepthAlarm;
		SF_INFO      infoWave;
		SF_INFO      infoEngine;
		SF_INFO      infoHorn;
		SF_INFO      infoAlarm;
		//SOUND OUTSIDE AND INSIDE BOAT KYARA
		SF_INFO  infoInside;
		SF_INFO  infoOutside;
		//KYARA SOUND SYSTEM PROXY AND COLLISION
		SF_INFO      infoProxy;
		SF_INFO infoRadarAlarm;
		SF_INFO      infoCollision;
		//KYARA SEAGULL
		SF_INFO      infoSeagull;
		SF_INFO      infoThunderbolt;
		//KYARA CONTACT
		SF_INFO      infoContact;
	} callback_data_s;

	static float hornVolume;
	static float waveVolume;
	static float engineVolume;
	static float alarmVolume;
	//storm
	static float rainVolume;    static bool rainSoundLoaded;
	static float stormVolume;   static bool stormSoundLoaded;
	static float thunderVolume; static bool thunderSoundLoaded; static double thunderPosSeconds;
	//KYARA ENGINE PITCH
	static float enginePitch; // playback-rate multiplier, 1.0 = native pitch
	//SOUND OUTSIDE AND INSIDE BOAT KYARA
	static float insideVolume;
	static float outsideVolume;
	//KYARA SOUND SYSTEM PROXY AND COLLISION
	static float proxyVolume;
	static float radarAlarmVolume;
	static float collisionVolume;
	static float thunderboltVolume;
	//KYARA SEAGULL
	static float seagullVolume;
	//KYARA CONTACT
	static float contactVolume;

	bool soundLoaded;
	static bool waveSoundLoaded;
	static bool hornSoundLoaded;
	static bool alarmSoundLoaded;
	//SOUND OUTSIDE AND INSIDE BOAT KYARA
	static bool insideSoundLoaded;
	static bool outsideSoundLoaded;
	//KYARA SOUND SYSTEM PROXY AND COLLISION
	static bool proxySoundLoaded;
	static bool radarAlarmSoundLoaded;
	static bool thunderboltSoundLoaded;
	static bool collisionSoundLoaded;
	//KYARA SEAGULL
	static bool seagullSoundLoaded;
	//KYARA CONTACT
	static bool contactSoundLoaded;
	//KYARA: collision one-shot control. collisionTriggered is set by the main thread
	//(triggerCollision) and consumed by the audio callback; collisionPlaying is owned
	//by the callback and is true only while the single play-through is in progress.
	static volatile bool collisionTriggered;
	static bool collisionPlaying;
	static volatile bool thunderboltTriggered;
	static bool thunderboltPlaying;
	//KYARA SEAGULL: same one-shot pattern as collision, see comment above.
	static volatile bool seagullTriggered;
	static bool seagullPlaying;
	//KYARA CONTACT: one-shot state, same pattern as collision/seagull.
	static volatile bool contactTriggered;
	static bool contactPlaying;
	//FIRE FEATURE
	static float fireBurningVolume; static bool fireBurningSoundLoaded;
	static float waterVolume;       static bool waterSoundLoaded;
	static float fireAlarmVolume;   static bool fireAlarmSoundLoaded;
	static volatile bool fireAlarmTriggered; static bool fireAlarmPlaying;
	static float abandonAlarmVolume; static bool abandonAlarmSoundLoaded;
	static float groanVolume; static bool groanSoundLoaded;
	static float steamVolume; static bool steamSoundLoaded;
	static float explosionVolume; static bool explosionSoundLoaded; static volatile bool explosionTriggered; static bool explosionPlaying;
	static float vhfVolume; static bool vhfSoundLoaded; static volatile bool vhfTriggered; static bool vhfPlaying;
	static float heloVolume; static bool heloSoundLoaded;
	//KYARA SLAM: one voice, one-shot, resampled. slamTriggered/slamGain/slamPitch are written by
	//the main thread and consumed by the callback; the *Playing values are owned by the callback
	//so a new trigger can never change the gain of a sound already in flight.
	static bool slamSoundLoaded;
	static float slamVolume;
	static bool cpaAlarmSoundLoaded;
	static float cpaAlarmVolume;
	static bool cpaAlarmWasOn;               // callback only: restart the pattern when it begins
	static bool depthAlarmSoundLoaded;
	static float depthAlarmVolume;
	static bool depthAlarmWasOn;             // callback only: restart the pattern when it begins
	static volatile bool slamTriggered;
	static volatile float slamGain;
	static volatile float slamPitch;
	static bool slamPlaying;
	static float slamGainPlaying;
	static float slamPitchPlaying;
	static double slamPhase;                 // fractional frame index, as for the engine channel
	static std::vector<float> slamScratch;   // raw frames, reused between callbacks (no allocation)
	//KYARA ENGINE PITCH: resampling state for the engine channel. enginePhase is the
	//fractional read position (in frames) into a rolling scratch buffer of raw engine
	//samples; engineScratch holds the most recently decoded raw frames so we can
	//interpolate between them at an arbitrary (non-1.0) playback rate. This lives across
	//callback invocations (hence static), so pitch changes are continuous and click-free.
	static std::vector<float> engineScratch; // raw frames read straight from file, interleaved

	static double enginePhase;               // fractional frame index into engineScratch
	static sf_count_t engineScratchValidFrames; // how many frames in engineScratch are valid this buffer


	PaError portAudioError;
	PaStream* stream;
	//SNDFILE *file;
	callback_data_s data;

	static int callback
	(const void* input
		, void* output
		, unsigned long                   frameCount
		, const PaStreamCallbackTimeInfo* timeInfo
		, PaStreamCallbackFlags           statusFlags
		, void* userData
	)
	{
		float* out;
		callback_data_s* p_data = (callback_data_s*)userData;
		sf_count_t       num_read;

		out = (float*)output;
		p_data = (callback_data_s*)userData;

		//Note that we've ensured already that channels are the same for all files, if both waveSoundLoaded and hornSoundLoaded are true

		/* clear output buffer */
		memset(out, 0, sizeof(float) * frameCount * p_data->infoEngine.channels);
		std::vector<float> abandonAlarmBuffer(frameCount * p_data->infoEngine.channels, 0.0f);
		//Create buffers, for wave, engine, horn and alarm
		std::vector<float> engineBuffer(frameCount * p_data->infoEngine.channels, 0.0f); // filled by the pitch-resampling block below
		std::vector<float> waveBuffer(sizeof(float) * frameCount * p_data->infoEngine.channels);
		std::vector<float> hornBuffer(sizeof(float) * frameCount * p_data->infoEngine.channels);
		std::vector<float> alarmBuffer(sizeof(float) * frameCount * p_data->infoEngine.channels);
		std::vector<float> thunderboltBuffer(sizeof(float) * frameCount * p_data->infoEngine.channels);
		//KYARA COLLISION AND PROXY 
		std::vector<float> proxyBuffer(sizeof(float) * frameCount * p_data->infoEngine.channels);
		std::vector<float> radarAlarmBuffer(sizeof(float) * frameCount * p_data->infoEngine.channels);
		std::vector<float> collisionBuffer(sizeof(float) * frameCount * p_data->infoEngine.channels);
		//KYARA SEAGULL
		std::vector<float> seagullBuffer(sizeof(float) * frameCount * p_data->infoEngine.channels);
		//KYARA CONTACT: zero-initialised, so the unread tail of the final partial
		//buffer of the one-shot mixes as silence.
		std::vector<float> contactBuffer(frameCount * p_data->infoEngine.channels, 0.0f);
		//FIRE FEATURE
		std::vector<float> fireBurningBuffer(frameCount * p_data->infoEngine.channels, 0.0f);
		std::vector<float> waterBuffer(frameCount * p_data->infoEngine.channels, 0.0f);
		std::vector<float> fireAlarmBuffer(frameCount * p_data->infoEngine.channels, 0.0f);
		std::vector<float> explosionBuffer(frameCount * p_data->infoEngine.channels, 0.0f);
		std::vector<float> groanBuffer(frameCount * p_data->infoEngine.channels, 0.0f);
		std::vector<float> steamBuffer(frameCount * p_data->infoEngine.channels, 0.0f);
		std::vector<float> vhfBuffer(frameCount * p_data->infoEngine.channels, 0.0f);
		std::vector<float> heloBuffer(frameCount * p_data->infoEngine.channels, 0.0f);
		//KYARA SLAM: zero-filled, so the tail of the final partial buffer mixes as silence
		std::vector<float> slamBuffer(frameCount * p_data->infoEngine.channels, 0.0f);
		std::vector<float> cpaAlarmBuffer(frameCount * p_data->infoEngine.channels, 0.0f);
		std::vector<float> depthAlarmBuffer(frameCount * p_data->infoEngine.channels, 0.0f);
		//ANGLE SOUND CHANGE KYARA 
		//SOUND OUTSIDE AND INSIDE BOAT KYARA
		// SOUND OUTSIDE AND INSIDE BOAT KYARA - Corrected buffer size
		std::vector<float> insideBuffer(frameCount * p_data->infoEngine.channels, 0.0f);
		std::vector<float> outsideBuffer(frameCount * p_data->infoEngine.channels, 0.0f);

		//STORM
		std::vector<float> rainBuffer(frameCount * p_data->infoEngine.channels, 0.0f);
		std::vector<float> stormBuffer(frameCount * p_data->infoEngine.channels, 0.0f);
		std::vector<float> thunderBuffer(frameCount * p_data->infoEngine.channels, 0.0f);

		//KYARA ENGINE PITCH: read the engine channel at a variable rate so throttle changes
		//pitch-shift the sound (turbo whine on the way up), rather than just changing volume.
		//
		// enginePitch is a playback-rate multiplier (1.0 = recorded pitch). For each output
		// frame n (0..frameCount-1) we want source frame (enginePhase + n*pitch). We keep a
		// rolling "scratch" buffer of raw decoded frames ahead of the current phase, refilling
		// it (and looping the file at EOF, exactly as the old fixed-rate path did) whenever the
		// next required source index would run past what's buffered. Linear interpolation
		// between adjacent source frames avoids the clicking that nearest-neighbour resampling
		// would cause.
		{
			const unsigned int channels = p_data->infoEngine.channels;
			const float pitch = (enginePitch > 0.01f) ? enginePitch : 0.01f; // guard against 0/negative

			// How many source frames could this output buffer possibly need, worst case.
			// +2 gives headroom for the interpolation's "next sample" lookup at the tail.
			const sf_count_t framesNeeded = (sf_count_t)(frameCount * pitch) + 2;

			// (Re)fill engineScratch so it has at least framesNeeded frames available from
			// the *current* fractional phase onward. We keep things simple by always reloading
			// a fresh scratch window sized to this buffer's needs, anchored so that index 0
			// of the new window corresponds to floor(enginePhase) of the old window - i.e. we
			// don't lose the fractional part of the phase across callback boundaries.
			double phaseWhole;
			double phaseFrac = std::modf(enginePhase, &phaseWhole);

			// Seek the file to the integer part of the phase relative to where we logically are.
			// We track position implicitly: after filling, enginePhase is reset to phaseFrac,
			// and the scratch buffer's frame 0 represents that whole-frame position. So here we
			// just need to read framesNeeded frames forward from "now", with looping on EOF.
			engineScratch.assign((framesNeeded + 1) * channels, 0.0f);
			sf_count_t framesRead = 0;
			while (framesRead < framesNeeded + 1) {
				sf_count_t got = sf_read_float(
					p_data->fileEngine,
					engineScratch.data() + framesRead * channels,
					(framesNeeded + 1 - framesRead) * channels) / channels;
				if (got <= 0) {
					// EOF (or error): loop back to the start and keep filling.
					if (sf_seek(p_data->fileEngine, 0, SEEK_SET) == -1) {
						break; // can't loop - leave the remainder zeroed (silence), don't kill the stream
					}
					continue;
				}
				framesRead += got;
			}
			engineScratchValidFrames = framesRead;

			std::fill(engineBuffer.begin(), engineBuffer.end(), 0.0f);
			for (unsigned long n = 0; n < frameCount; n++) {
				double srcPos = phaseFrac + (double)n * pitch;
				sf_count_t i0 = (sf_count_t)srcPos;
				double frac = srcPos - (double)i0;
				sf_count_t i1 = i0 + 1;
				if (i1 >= engineScratchValidFrames) { i1 = engineScratchValidFrames > 0 ? engineScratchValidFrames - 1 : 0; }
				if (i0 >= engineScratchValidFrames) { i0 = engineScratchValidFrames > 0 ? engineScratchValidFrames - 1 : 0; }
				for (unsigned int c = 0; c < channels; c++) {
					float s0 = engineScratch[i0 * channels + c];
					float s1 = engineScratch[i1 * channels + c];
					engineBuffer[n * channels + c] = (float)(s0 + (s1 - s0) * frac);
				}
			}

			// Advance the phase by exactly what we consumed, carrying the fractional remainder
			// into the next callback. Then rewind the file by the frames we buffered but didn't
			// use, so the next callback's fresh read starts exactly where this one left off.
			double consumed = phaseFrac + (double)frameCount * pitch;
			sf_count_t consumedWhole = (sf_count_t)consumed;
			enginePhase = consumed - (double)consumedWhole;
			sf_count_t unusedFrames = engineScratchValidFrames - consumedWhole;
			if (unusedFrames > 0) {
				sf_seek(p_data->fileEngine, -unusedFrames, SEEK_CUR);
			}
		}
		num_read = frameCount; // engine channel is always fully populated (loops on EOF above)

		if (waveSoundLoaded) {
			num_read = sf_read_float(p_data->fileWave, waveBuffer.data(), frameCount * p_data->infoEngine.channels);
			/*  If we couldn't read a full frameCount of samples we've reached EOF */
			//Try to restart
			if (num_read < frameCount * p_data->infoEngine.channels)
			{

				sf_count_t seekLocation = sf_seek(p_data->fileWave, 0, SEEK_SET);
				if (seekLocation == -1) {
					waveSoundLoaded = false; //unreadable: this sound only, the others go on
				}

				//Read again
				/* read directly into output buffer */
				num_read = sf_read_float(p_data->fileWave, waveBuffer.data(), frameCount * p_data->infoEngine.channels);

				/*  If we couldn't read a full frameCount of samples we've reached EOF */
				if (num_read < frameCount) {
					waveSoundLoaded = false; //unreadable: this sound only, the others go on
				}
			}
		}

		if (hornSoundLoaded) {
			num_read = sf_read_float(p_data->fileHorn, hornBuffer.data(), frameCount * p_data->infoEngine.channels);
			/*  If we couldn't read a full frameCount of samples we've reached EOF */
			//Try to restart
			if (num_read < frameCount * p_data->infoEngine.channels)
			{

				sf_count_t seekLocation = sf_seek(p_data->fileHorn, 0, SEEK_SET);
				if (seekLocation == -1) {
					hornSoundLoaded = false; //unreadable: this sound only, the others go on
				}

				//Read again
				/* read directly into output buffer */
				num_read = sf_read_float(p_data->fileHorn, hornBuffer.data(), frameCount * p_data->infoEngine.channels);

				/*  If we couldn't read a full frameCount of samples we've reached EOF */
				if (num_read < frameCount) {
					hornSoundLoaded = false; //unreadable: this sound only, the others go on
				}
			}
		}

		if (alarmSoundLoaded) {
			num_read = sf_read_float(p_data->fileAlarm, alarmBuffer.data(), frameCount * p_data->infoEngine.channels);
			/*  If we couldn't read a full frameCount of samples we've reached EOF */
			//Try to restart
			if (num_read < frameCount * p_data->infoEngine.channels)
			{

				sf_count_t seekLocation = sf_seek(p_data->fileAlarm, 0, SEEK_SET);
				if (seekLocation == -1) {
					alarmSoundLoaded = false; //unreadable: this sound only, the others go on
				}

				//Read again
				/* read directly into output buffer */
				num_read = sf_read_float(p_data->fileAlarm, alarmBuffer.data(), frameCount * p_data->infoEngine.channels);

				/*  If we couldn't read a full frameCount of samples we've reached EOF */
				if (num_read < frameCount) {
					alarmSoundLoaded = false; //unreadable: this sound only, the others go on
				}
			}
		}
		// KYARA RAIN: looping, same failure-tolerant model as proxy.
		bool playRain = rainSoundLoaded;
		if (playRain) {
			num_read = sf_read_float(p_data->fileRain, rainBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount * p_data->infoEngine.channels) {
				if (sf_seek(p_data->fileRain, 0, SEEK_SET) == -1) { playRain = false; }
				else {
					num_read = sf_read_float(p_data->fileRain, rainBuffer.data(), frameCount * p_data->infoEngine.channels);
					if (num_read < frameCount * p_data->infoEngine.channels) { playRain = false; }
				}
			}
		}
		// KYARA STORM: looping ambience, same model.
		bool playStorm = stormSoundLoaded;
		if (playStorm) {
			num_read = sf_read_float(p_data->fileStorm, stormBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount * p_data->infoEngine.channels) {
				if (sf_seek(p_data->fileStorm, 0, SEEK_SET) == -1) { playStorm = false; }
				else {
					num_read = sf_read_float(p_data->fileStorm, stormBuffer.data(), frameCount * p_data->infoEngine.channels);
					if (num_read < frameCount * p_data->infoEngine.channels) { playStorm = false; }
				}
			}
		}
		//KYARA UPDATE
		// Proxy alarm: loops continuously while active. A read/seek failure just skips
		// it for this buffer (never returns paComplete, which would kill the whole stream).
		bool playProxy = proxySoundLoaded;
		if (playProxy) {
			num_read = sf_read_float(p_data->fileProxy, proxyBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount * p_data->infoEngine.channels)
			{
				if (sf_seek(p_data->fileProxy, 0, SEEK_SET) == -1) {
					playProxy = false;
				}
				else {
					num_read = sf_read_float(p_data->fileProxy, proxyBuffer.data(), frameCount * p_data->infoEngine.channels);
					if (num_read < frameCount * p_data->infoEngine.channels) {
						playProxy = false;
					}
				}
			}
		}

		// KYARA RADAR GUARD ALARM: independent looping channel, same model as proxy.
		bool playRadarAlarm = radarAlarmSoundLoaded;
		if (playRadarAlarm) {
			num_read = sf_read_float(p_data->fileRadarAlarm, radarAlarmBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount * p_data->infoEngine.channels)
			{
				if (sf_seek(p_data->fileRadarAlarm, 0, SEEK_SET) == -1) {
					playRadarAlarm = false;
				}
				else {
					num_read = sf_read_float(p_data->fileRadarAlarm, radarAlarmBuffer.data(), frameCount * p_data->infoEngine.channels);
					if (num_read < frameCount * p_data->infoEngine.channels) {
						playRadarAlarm = false;
					}
				}
			}
		}

		// Radar CPA/TCPA alarm: looping, read only while it sounds, from the start of the pattern each
		// time it begins; the end of the file runs on into its start, so the loop has no gap.
		bool playCpaAlarm = cpaAlarmSoundLoaded && cpaAlarmVolume > 0.0f;
		if (playCpaAlarm) {
			if (!cpaAlarmWasOn) { sf_seek(p_data->fileCpaAlarm, 0, SEEK_SET); }
			const sf_count_t wanted = frameCount * p_data->infoEngine.channels;
			sf_count_t got = sf_read_float(p_data->fileCpaAlarm, cpaAlarmBuffer.data(), wanted);
			if (got < wanted && sf_seek(p_data->fileCpaAlarm, 0, SEEK_SET) != -1) {
				got += sf_read_float(p_data->fileCpaAlarm, cpaAlarmBuffer.data() + got, wanted - got);
			}
			if (got <= 0) { playCpaAlarm = false; }
		}
		cpaAlarmWasOn = playCpaAlarm;

		// Echo sounder alarm: same looping model as the CPA alarm
		bool playDepthAlarm = depthAlarmSoundLoaded && depthAlarmVolume > 0.0f;
		if (playDepthAlarm) {
			if (!depthAlarmWasOn) { sf_seek(p_data->fileDepthAlarm, 0, SEEK_SET); }
			const sf_count_t wanted = frameCount * p_data->infoEngine.channels;
			sf_count_t got = sf_read_float(p_data->fileDepthAlarm, depthAlarmBuffer.data(), wanted);
			if (got < wanted && sf_seek(p_data->fileDepthAlarm, 0, SEEK_SET) != -1) {
				got += sf_read_float(p_data->fileDepthAlarm, depthAlarmBuffer.data() + got, wanted - got);
			}
			if (got <= 0) { playDepthAlarm = false; }
		}
		depthAlarmWasOn = playDepthAlarm;

		// Collision: ONE-SHOT. It only plays after triggerCollision() is called, plays
		// through a single time, then goes silent (no looping). A short collision file
		// can therefore never stop the stream.
		if (collisionTriggered) {
			sf_seek(p_data->fileCollision, 0, SEEK_SET); // restart from the beginning
			collisionTriggered = false;
			collisionPlaying = true;
		}
		bool playCollision = false;
		if (collisionSoundLoaded && collisionPlaying) {
			num_read = sf_read_float(p_data->fileCollision, collisionBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount) {
				// End of the one-shot: this is the final (partial) buffer. The unread
				// tail of collisionBuffer is already zero (vector was value-initialised),
				// so we can mix it as-is, then stop - do NOT loop back to the start.
				collisionPlaying = false;
			}
			playCollision = true;
		}
		// KYARA THUNDER: looping sequence, same model as storm.
		bool playThunder = thunderSoundLoaded;
		if (playThunder) {
			num_read = sf_read_float(p_data->fileThunder, thunderBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount * p_data->infoEngine.channels) {
				if (sf_seek(p_data->fileThunder, 0, SEEK_SET) == -1) { playThunder = false; }
				else {
					num_read = sf_read_float(p_data->fileThunder, thunderBuffer.data(), frameCount * p_data->infoEngine.channels);
					if (num_read < frameCount * p_data->infoEngine.channels) { playThunder = false; }
				}
			}
		}
		// KYARA: expose current position in the thunder loop (seconds) for synced lightning.
		if (thunderSoundLoaded) {
			thunderPosSeconds = (double)sf_seek(p_data->fileThunder, 0, SEEK_CUR) / (double)p_data->infoEngine.samplerate;
		}
		// KYARA SEAGULL: same one-shot pattern as collision above - triggered by a key
		// press (triggerSeagull()), plays through once, then goes silent.
		if (seagullTriggered) {
			sf_seek(p_data->fileSeagull, 0, SEEK_SET);
			seagullTriggered = false;
			seagullPlaying = true;
		}
		bool playSeagull = false;
		if (seagullSoundLoaded && seagullPlaying) {
			num_read = sf_read_float(p_data->fileSeagull, seagullBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount) {
				seagullPlaying = false;
			}
			playSeagull = true;

		}


		// NAUTITECH HERO BOLT: one-shot, identical model to the seagull.
		if (thunderboltTriggered) {
			sf_seek(p_data->fileThunderbolt, 0, SEEK_SET);
			thunderboltTriggered = false;
			thunderboltPlaying = true;
		}
		bool playThunderbolt = false;
		if (thunderboltSoundLoaded && thunderboltPlaying) {
			num_read = sf_read_float(p_data->fileThunderbolt, thunderboltBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount) {
				thunderboltPlaying = false;
			}
			playThunderbolt = true;
		}
		// KYARA CONTACT: one-shot metal contact with a quay/fender. Identical playback
		// model to the collision one-shot above - no looping, so a short sample can never
		// stop the stream.
		if (contactTriggered) {
			sf_seek(p_data->fileContact, 0, SEEK_SET);
			contactTriggered = false;
			contactPlaying = true;
		}
		bool playContact = false;
		if (contactSoundLoaded && contactPlaying) {
			num_read = sf_read_float(p_data->fileContact, contactBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount) {
				contactPlaying = false;
			}
			playContact = true;
		}
		// FIRE FEATURE — fireBurning: continuous loop (same model as proxy)
		bool playFireBurning = fireBurningSoundLoaded;
		if (playFireBurning) {
			num_read = sf_read_float(p_data->fileFireBurning, fireBurningBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount * p_data->infoEngine.channels) {
				if (sf_seek(p_data->fileFireBurning, 0, SEEK_SET) == -1) { playFireBurning = false; }
				else {
					num_read = sf_read_float(p_data->fileFireBurning, fireBurningBuffer.data(), frameCount * p_data->infoEngine.channels);
					if (num_read < frameCount * p_data->infoEngine.channels) { playFireBurning = false; }
				}
			}
		}
		// abandon ship alarm: continuous loop, controlled by volume
		bool playAbandonAlarm = abandonAlarmSoundLoaded;
		if (playAbandonAlarm) {
			num_read = sf_read_float(p_data->fileAbandonAlarm, abandonAlarmBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount * p_data->infoEngine.channels) {
				if (sf_seek(p_data->fileAbandonAlarm, 0, SEEK_SET) == -1) { playAbandonAlarm = false; }
				else {
					num_read = sf_read_float(p_data->fileAbandonAlarm, abandonAlarmBuffer.data(), frameCount * p_data->infoEngine.channels);
					if (num_read < frameCount * p_data->infoEngine.channels) { playAbandonAlarm = false; }
				}
			}
		}
		// FIRE FEATURE — water: continuous loop (same model as proxy)
		bool playWater = waterSoundLoaded;
		if (playWater) {
			num_read = sf_read_float(p_data->fileWater, waterBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount * p_data->infoEngine.channels) {
				if (sf_seek(p_data->fileWater, 0, SEEK_SET) == -1) { playWater = false; }
				else {
					num_read = sf_read_float(p_data->fileWater, waterBuffer.data(), frameCount * p_data->infoEngine.channels);
					if (num_read < frameCount * p_data->infoEngine.channels) { playWater = false; }
				}
			}
		}
		// FIRE FEATURE — fireAlarm: continuous loop, controlled by volume (0 = silent)
		bool playFireAlarm = fireAlarmSoundLoaded;
		if (playFireAlarm) {
			num_read = sf_read_float(p_data->fileFireAlarm, fireAlarmBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount * p_data->infoEngine.channels) {
				if (sf_seek(p_data->fileFireAlarm, 0, SEEK_SET) == -1) { playFireAlarm = false; }
				else {
					num_read = sf_read_float(p_data->fileFireAlarm, fireAlarmBuffer.data(), frameCount * p_data->infoEngine.channels);
					if (num_read < frameCount * p_data->infoEngine.channels) { playFireAlarm = false; }
				}
			}
		}
		// FIRE: groan (structural metal) — loop
		bool playGroan = groanSoundLoaded;
		if (playGroan) {
			num_read = sf_read_float(p_data->fileGroan, groanBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount * p_data->infoEngine.channels) {
				if (sf_seek(p_data->fileGroan, 0, SEEK_SET) == -1) { playGroan = false; }
				else { num_read = sf_read_float(p_data->fileGroan, groanBuffer.data(), frameCount * p_data->infoEngine.channels); if (num_read < frameCount) playGroan = false; }
			}
		}
		// FIRE: steam (knock-down) — loop
		bool playSteam = steamSoundLoaded;
		if (playSteam) {
			num_read = sf_read_float(p_data->fileSteam, steamBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount * p_data->infoEngine.channels) {
				if (sf_seek(p_data->fileSteam, 0, SEEK_SET) == -1) { playSteam = false; }
				else { num_read = sf_read_float(p_data->fileSteam, steamBuffer.data(), frameCount * p_data->infoEngine.channels); if (num_read < frameCount) playSteam = false; }
			}
		}
		// FIRE: explosion — one-shot (same model as seagull)
		if (explosionTriggered) { sf_seek(p_data->fileExplosion, 0, SEEK_SET); explosionTriggered = false; explosionPlaying = true; }
		bool playExplosion = false;
		if (explosionSoundLoaded && explosionPlaying) {
			num_read = sf_read_float(p_data->fileExplosion, explosionBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount * p_data->infoEngine.channels) { explosionPlaying = false; }
			playExplosion = true;
		}
		// FIRE: vhf beep — one-shot
				// FIRE: vhf radio alarm — loop, controlled by volume (plays until the fire is out)
		bool playVhf = vhfSoundLoaded;
		if (playVhf) {
			num_read = sf_read_float(p_data->fileVhf, vhfBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount * p_data->infoEngine.channels) {
				if (sf_seek(p_data->fileVhf, 0, SEEK_SET) == -1) { playVhf = false; }
				else { num_read = sf_read_float(p_data->fileVhf, vhfBuffer.data(), frameCount * p_data->infoEngine.channels); if (num_read < frameCount) playVhf = false; }
			}
		}
		// SAR: helicopter rotor - loop, controlled by volume
		bool playHelo = heloSoundLoaded;
		if (playHelo) {
			num_read = sf_read_float(p_data->fileHelo, heloBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount * p_data->infoEngine.channels) {
				if (sf_seek(p_data->fileHelo, 0, SEEK_SET) == -1) { playHelo = false; }
				else { num_read = sf_read_float(p_data->fileHelo, heloBuffer.data(), frameCount * p_data->infoEngine.channels); if (num_read < frameCount) playHelo = false; }
			}
		}
		// KYARA SLAM: one-shot, read at a variable rate so each impact can be pitched. Same
		// resampling scheme as the engine channel, but it stops at EOF instead of looping.
		if (slamTriggered) {
			if (slamSoundLoaded) {
				sf_seek(p_data->fileSlam, 0, SEEK_SET);
				slamGainPlaying = slamGain;
				slamPitchPlaying = slamPitch;
				slamPhase = 0.0;
				slamPlaying = true;
			}
			slamTriggered = false;
		}
		bool playSlam = false;
		if (slamPlaying && slamSoundLoaded) {
			const unsigned int channels = p_data->infoEngine.channels;
			SNDFILE* fSlam = p_data->fileSlam;
			float pitch = slamPitchPlaying;
			if (pitch < 0.25f) { pitch = 0.25f; }
			if (pitch > 4.0f) { pitch = 4.0f; }

			const sf_count_t framesNeeded = (sf_count_t)(frameCount * pitch) + 2;
			slamScratch.assign((framesNeeded + 1) * channels, 0.0f);
			sf_count_t framesRead = 0;
			bool hitEof = false;
			while (framesRead < framesNeeded + 1) {
				sf_count_t got = sf_read_float(fSlam,
					slamScratch.data() + framesRead * channels,
					(framesNeeded + 1 - framesRead) * channels) / channels;
				if (got <= 0) { hitEof = true; break; }   // end of the sample: no looping
				framesRead += got;
			}

			if (framesRead <= 0) {
				slamPlaying = false;                       // nothing left to play
			}
			else {
				const double phaseFrac = slamPhase;
				for (unsigned long n = 0; n < frameCount; n++) {
					double srcPos = phaseFrac + (double)n * pitch;
					sf_count_t i0 = (sf_count_t)srcPos;
					double frac = srcPos - (double)i0;
					sf_count_t i1 = i0 + 1;
					if (i0 >= framesRead) { i0 = framesRead - 1; frac = 0.0; }
					if (i1 >= framesRead) { i1 = framesRead - 1; }
					for (unsigned int c = 0; c < channels; c++) {
						float s0 = slamScratch[i0 * channels + c];
						float s1 = slamScratch[i1 * channels + c];
						slamBuffer[n * channels + c] = (float)(s0 + (s1 - s0) * frac);
					}
				}
				double consumed = phaseFrac + (double)frameCount * pitch;
				sf_count_t consumedWhole = (sf_count_t)consumed;
				slamPhase = consumed - (double)consumedWhole;
				if (hitEof) {
					if (consumedWhole >= framesRead) { slamPlaying = false; } // finished
				}
				else {
					sf_count_t unusedFrames = framesRead - consumedWhole;
					if (unusedFrames > 0) { sf_seek(fSlam, -unusedFrames, SEEK_CUR); }
				}
				playSlam = true;
			}
		}

		// SOUND OUTSIDE AND INSIDE BOAT KYARA: Seamless continuous looping
		if (insideSoundLoaded) {
			sf_count_t num_read = sf_read_float(p_data->fileInside, insideBuffer.data(), frameCount * p_data->infoEngine.channels);
			// If we hit the end of the file, loop back to the start and fill the rest of the buffer
			if (num_read < frameCount * p_data->infoEngine.channels) {
				sf_seek(p_data->fileInside, 0, SEEK_SET);
				sf_read_float(p_data->fileInside, insideBuffer.data() + num_read, (frameCount * p_data->infoEngine.channels) - num_read);
			}
		}

		if (outsideSoundLoaded) {
			sf_count_t num_read = sf_read_float(p_data->fileOutside, outsideBuffer.data(), frameCount * p_data->infoEngine.channels);
			if (num_read < frameCount * p_data->infoEngine.channels) {
				sf_seek(p_data->fileOutside, 0, SEEK_SET);
				sf_read_float(p_data->fileOutside, outsideBuffer.data() + num_read, (frameCount * p_data->infoEngine.channels) - num_read);
			}
		}

		//Copy into output buffer, with mixing
		for (int i = 0; i < frameCount * p_data->infoEngine.channels; i++) { //(every buffer is in the engine sound's format)
			out[i] = engineVolume * engineBuffer[i] * 0.33;
			if (waveSoundLoaded) {
				out[i] += waveVolume * waveBuffer[i] * 0.33;
			}
			if (hornSoundLoaded) {
				out[i] += hornVolume * hornBuffer[i] * 0.33;
			}
			if (alarmSoundLoaded) {
				out[i] += alarmVolume * alarmBuffer[i] * 0.33;
			}
			//KYARA COLLISION + PROXY
			if (playProxy) {
				out[i] += proxyVolume * proxyBuffer[i] * 0.33;
			}
			if (playRadarAlarm) {
				out[i] += radarAlarmVolume * radarAlarmBuffer[i] * 0.5;
			}
			if (playCpaAlarm) {
				out[i] += cpaAlarmVolume * cpaAlarmBuffer[i] * 0.5;
			}
			if (playDepthAlarm) {
				out[i] += depthAlarmVolume * depthAlarmBuffer[i] * 0.5;
			}
			if (playCollision) {
				out[i] += collisionVolume * collisionBuffer[i] * 0.33;
			}
			//KYARA SEAGULL
			if (playSeagull) {
				out[i] += seagullVolume * seagullBuffer[i] * 0.33;
			}
			//FIRE FEATURE
			if (playFireBurning) { out[i] += fireBurningVolume * fireBurningBuffer[i] * 0.33; }
			if (playWater) { out[i] += waterVolume * waterBuffer[i] * 0.33; }
			if (playFireAlarm) { out[i] += fireAlarmVolume * fireAlarmBuffer[i] * 0.9; }
			if (playGroan) { out[i] += groanVolume * groanBuffer[i] * 0.5; }
			if (playSteam) { out[i] += steamVolume * steamBuffer[i] * 0.4; }
			if (playExplosion) { out[i] += explosionVolume * explosionBuffer[i] * 0.9; }
			if (playVhf) { out[i] += vhfVolume * vhfBuffer[i] * 0.25; }
			if (playHelo) { out[i] += heloVolume * heloBuffer[i] * 0.7; }
			//NAUTITECH HERO BOLT
			if (playThunderbolt) {
				out[i] += thunderboltVolume * thunderboltBuffer[i] * 0.8;
			}
			//abandon ship volume   
			if (playAbandonAlarm) { out[i] += abandonAlarmVolume * abandonAlarmBuffer[i] * 1.0; }
			//KYARA SLAM: per-impact gain on top of the channel volume
			if (playSlam) {
				out[i] += slamVolume * slamGainPlaying * slamBuffer[i] * 0.9f;
			}
			//KYARA CONTACT
			if (playContact) {
				out[i] += contactVolume * contactBuffer[i] * 0.8;
			}
			//STORM
			if (playRain) { out[i] += rainVolume * rainBuffer[i] * 0.33; }
			if (playStorm) { out[i] += stormVolume * stormBuffer[i] * 0.33; }
			if (playThunder) { out[i] += thunderVolume * thunderBuffer[i] * 0.7; }
			//SOUND OUTSIDE AND INSIDE BOAT KYARA
		// Use the same 0.33 mix coefficient as all other channels so the
		// inside/outside sounds are not 50 % louder than the engine/wave/horn.
			if (insideSoundLoaded) {
				out[i] += insideVolume * insideBuffer[i] * 0.33f;
			}
			if (outsideSoundLoaded) {
				out[i] += outsideVolume * outsideBuffer[i] * 0.33f;
			}
		}

		return paContinue;
	}

#endif // WITH_SOUND

};

#endif