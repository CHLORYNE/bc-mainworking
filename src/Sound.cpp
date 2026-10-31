#include "Sound.hpp"
#include <random>

#ifndef WITH_SOUND
//Dummy implementation of public interface

Sound::Sound() {}
Sound::~Sound() {}
//KYARA COLLISION PROXY
void Sound::load(std::string engineSoundFile, std::string waveSoundFile, std::string hornSoundFile,
	std::string alarmSoundFile, std::string proxyAlarmSoundFile,
	std::string collisionSoundFile, std::string insideSoundFile,
	std::string outsideSoundFile, std::string seagullSoundFile,
	std::string contactSoundFile, std::string radarAlarmSoundFile, std::string rainSoundFile, std::string stormSoundFile, std::string thunderSoundFile, std::string thunderboltSoundFile,
	std::string fireBurningSoundFile, std::string waterSoundFile, std::string fireAlarmSoundFile, std::string abandonAlarmSoundFile,
	std::string explosionSoundFile, std::string groanSoundFile, std::string steamSoundFile, std::string vhfSoundFile) {
}
void Sound::StartSound() {}void Sound::setVolumeWave(float vol) {}
void Sound::setVolumeEngine(float vol) {}
void Sound::setVolumeHorn(float vol) {}
void Sound::setVolumeAlarm(float vol) {}
void Sound::setVolumeProxyAlarm(float vol) {}
void Sound::setVolumeRadarAlarm(float vol) {}
void Sound::setVolumeCollision(float vol) {}
void Sound::setPitchEngine(float pitch) {}
float Sound::getPitchEngine() const { return 1.0f; }
void Sound::setVolumeInside(float vol) {}   // dummy — required to match Sound.hpp
void Sound::setVolumeOutside(float vol) {}  // dummy — required to match Sound.hpp
void Sound::triggerCollision() {}
void Sound::setVolumeSeagull(float vol) {}
float Sound::getVolumeSeagull() const { return 0; }
void Sound::triggerSeagull() {}
void Sound::setVolumeFireBurning(float vol) {}  float Sound::getVolumeFireBurning() const { return 0; }
void Sound::setVolumeWater(float vol) {}        float Sound::getVolumeWater() const { return 0; }
void Sound::setVolumeFireAlarm(float vol) {}    float Sound::getVolumeFireAlarm() const { return 0; }
void Sound::triggerFireAlarm() {}
void Sound::triggerExplosion() {}
void Sound::setVolumeVhf(float vol) {}  float Sound::getVolumeVhf() const { return 0; }
void Sound::setVolumeHelo(float vol) {} float Sound::getVolumeHelo() const { return 0; }
void Sound::setVolumeAbandonAlarm(float vol) {}  float Sound::getVolumeAbandonAlarm() const { return 0; }
void Sound::setVolumeGroan(float vol) {}         float Sound::getVolumeGroan() const { return 0; }
void Sound::setVolumeSteam(float vol) {}         float Sound::getVolumeSteam() const { return 0; }
void Sound::triggerThunderbolt() {}
void Sound::setVolumeContact(float vol) {}
float Sound::getVolumeContact() const { return 0; }
void Sound::triggerContact() {}
//KYARA SLAM
void Sound::loadSlamSound(std::string slamFile) {}
void Sound::loadCpaAlarmSound(std::string cpaAlarmFile) {}
void Sound::setVolumeCpaAlarm(float vol) {}
void Sound::loadDepthAlarmSound(std::string depthAlarmFile) {}
void Sound::setVolumeDepthAlarm(float vol) {}
void Sound::loadFailureAlarmSound(std::string failureAlarmFile) {}
void Sound::setVolumeFailureAlarm(float vol) {}
void Sound::setVolumeSlam(float vol) {}
float Sound::getVolumeSlam() const { return 0; }
void Sound::triggerSlam(float gain, float pitch) {}
bool Sound::hasSlamSound() const { return false; }
void Sound::setVolumeSonar(float vol) {}
float Sound::getVolumeSonar() const { return 0; }
void Sound::triggerSonar(float gain, float pitch, int kind) {}
float Sound::getVolumeWave() const { return 0; }
float Sound::getVolumeEngine() const { return 0; }
float Sound::getVolumeHorn() const { return 0; }
float Sound::getVolumeAlarm() const { return 0; }
float Sound::getVolumeProxyAlarm() const { return 0; }
float Sound::getVolumeRadarAlarm() const { return 0; }
float Sound::getVolumeCollision() const { return 0; }
void  Sound::setVolumeRain(float vol) {}    float Sound::getVolumeRain() const { return 0; }
void  Sound::setVolumeStorm(float vol) {}   float Sound::getVolumeStorm() const { return 0; }
void  Sound::setVolumeThunder(float vol) {} float Sound::getVolumeThunder() const { return 0; }
double Sound::getThunderPositionSeconds() const { return 0.0; }

#else // WITH_SOUND

#include <iostream>

//Volumes, should be in range 0-1
float Sound::hornVolume = 0.0;
float Sound::waveVolume = 1.0;
float Sound::engineVolume = 0.0;
//KYARA ENGINE PITCH
float Sound::enginePitch = 1.0f;
float Sound::alarmVolume = 0.0;
//KYARA COLLISION PROXY
float Sound::proxyVolume = 0.0;
float Sound::radarAlarmVolume = 0.0;
float Sound::collisionVolume = 0.0;
//KYARA SEAGULL
float Sound::seagullVolume = 1.0f;
float Sound::thunderboltVolume = 1.0f;
bool Sound::thunderboltSoundLoaded = false;
//KYARA CONTACT
float Sound::contactVolume = 0.0;
bool Sound::contactSoundLoaded = false;
volatile bool Sound::contactTriggered = false;
bool Sound::contactPlaying = false;
//FIRE FEATURE
float Sound::fireBurningVolume = 0.0; bool Sound::fireBurningSoundLoaded = false;
float Sound::waterVolume = 0.0;       bool Sound::waterSoundLoaded = false;
float Sound::fireAlarmVolume = 0.0f;  bool Sound::fireAlarmSoundLoaded = false;
volatile bool Sound::fireAlarmTriggered = false; bool Sound::fireAlarmPlaying = false;
float Sound::abandonAlarmVolume = 0.0f; bool Sound::abandonAlarmSoundLoaded = false;
float Sound::groanVolume = 0.0f; bool Sound::groanSoundLoaded = false;
float Sound::steamVolume = 0.0f; bool Sound::steamSoundLoaded = false;
float Sound::explosionVolume = 1.0f; bool Sound::explosionSoundLoaded = false; volatile bool Sound::explosionTriggered = false; bool Sound::explosionPlaying = false;
float Sound::vhfVolume = 0.0f; bool Sound::vhfSoundLoaded = false; volatile bool Sound::vhfTriggered = false; bool Sound::vhfPlaying = false;
float Sound::heloVolume = 0.0f; bool Sound::heloSoundLoaded = false;
//KYARA SLAM
bool Sound::slamSoundLoaded = false;
float Sound::slamVolume = 1.0f;
bool Sound::cpaAlarmSoundLoaded = false;
float Sound::cpaAlarmVolume = 0.0f;
bool Sound::cpaAlarmWasOn = false;
bool Sound::depthAlarmSoundLoaded = false;
float Sound::depthAlarmVolume = 0.0f;
bool Sound::depthAlarmWasOn = false;
bool Sound::failureAlarmSoundLoaded = false;
float Sound::failureAlarmVolume = 0.0f;
bool Sound::failureAlarmWasOn = false;
volatile bool Sound::slamTriggered = false;
volatile float Sound::slamGain = 1.0f;
volatile float Sound::slamPitch = 1.0f;
bool Sound::slamPlaying = false;
float Sound::slamGainPlaying = 1.0f;
float Sound::slamPitchPlaying = 1.0f;
double Sound::slamPhase = 0.0;
std::vector<float> Sound::slamScratch;
std::vector<float> Sound::sonarSounds[Sound::SONAR_SOUNDS];
float Sound::sonarVolume = 1.0f;
Sound::SonarRequest Sound::sonarRequests[16];
std::atomic<unsigned> Sound::sonarRequestsWritten(0);
unsigned Sound::sonarRequestsRead = 0;
Sound::SonarVoice Sound::sonarVoices[6] = {};
//SOUND OUTSIDE AND INSIDE BOAT KYARA
float Sound::insideVolume = 0.0;
float Sound::outsideVolume = 0.0;
//STORM
float Sound::rainVolume = 0.0;    bool Sound::rainSoundLoaded = false;
float Sound::stormVolume = 0.0;   bool Sound::stormSoundLoaded = false;
float Sound::thunderVolume = 0.0; bool Sound::thunderSoundLoaded = false; double Sound::thunderPosSeconds = 0.0;

bool Sound::waveSoundLoaded = false;
bool Sound::hornSoundLoaded = false;
bool Sound::alarmSoundLoaded = false;
//KYARA COLLISION PROXY
bool Sound::proxySoundLoaded = false;
bool Sound::radarAlarmSoundLoaded = false;
bool Sound::collisionSoundLoaded = false;
//KYARA SEAGULL
bool Sound::seagullSoundLoaded = false;
//SOUND OUTSIDE AND INSIDE BOAT KYARA
bool Sound::insideSoundLoaded = false;
bool Sound::outsideSoundLoaded = false;
//KYARA: collision one-shot state
volatile bool Sound::collisionTriggered = false;
bool Sound::collisionPlaying = false;
volatile bool Sound::thunderboltTriggered = false;
bool Sound::thunderboltPlaying = false;
//KYARA SEAGULL: one-shot state, same pattern as collision
volatile bool Sound::seagullTriggered = false;
bool Sound::seagullPlaying = false;
//KYARA ENGINE PITCH: resampling state, defined once here since they're static members
std::vector<float> Sound::engineScratch;
double Sound::enginePhase = 0.0;
sf_count_t Sound::engineScratchValidFrames = 0;

Sound::Sound() {
	//Every handle null, so the destructor is safe when load() stops early (no audio device: the
	//other files were never opened and sf_close was given garbage - a crash on quitting)
	data = callback_data_s();
	stream = 0;
	soundLoaded = false;
	slamSoundLoaded = false;
	cpaAlarmSoundLoaded = false;
	depthAlarmSoundLoaded = false;
	failureAlarmSoundLoaded = false;
}
//UPDATED KYARA COLLISION AND PROXY
void Sound::load(std::string engineSoundFile, std::string waveSoundFile, std::string hornSoundFile,
	std::string alarmSoundFile, std::string proxyAlarmSoundFile,
	std::string collisionSoundFile, std::string insideSoundFile,
	std::string outsideSoundFile, std::string seagullSoundFile,
	std::string contactSoundFile, std::string radarAlarmSoundFile, std::string rainSoundFile, std::string stormSoundFile, std::string thunderSoundFile, std::string thunderboltSoundFile,
	std::string fireBurningSoundFile, std::string waterSoundFile, std::string fireAlarmSoundFile, std::string abandonAlarmSoundFile,
	std::string explosionSoundFile, std::string groanSoundFile, std::string steamSoundFile, std::string vhfSoundFile) {

	soundLoaded = false;

	char buf[1024];
	sf_command(NULL, SFC_GET_LIB_VERSION, buf, sizeof(buf));

	portAudioError = Pa_Initialize();
	if (portAudioError != paNoError) {
		std::cerr << "Pa_Initialize failed." << std::endl;
		std::cerr << "Error: " << Pa_GetErrorText(portAudioError) << std::endl;
		return;
	}
	//storm
	data.fileRain = 0;

	data.fileStorm = 0;
	data.fileThunder = 0;
	data.fileEngine = 0;
	data.fileWave = 0;
	data.fileHorn = 0;
	data.fileAlarm = 0;
	//KYARA UDPATE
	data.fileProxy = 0;
	data.fileRadarAlarm = 0;
	data.fileCollision = 0;
	//KYARA SEAGULL
	data.fileSeagull = 0;
	data.fileFireBurning = 0;
	data.fileWater = 0;
	data.fileFireAlarm = 0;
	data.fileAbandonAlarm = 0;
	data.fileExplosion = 0;
	data.fileGroan = 0;
	data.fileSteam = 0;
	data.fileVhf = 0;
	data.fileHelo = 0;
	data.fileThunderbolt = 0;
	//KYARA CONTACT QUAI
	data.fileContact = 0;
	// ANGLE CHANGE SOUND 
	data.fileInside = 0;
	data.fileOutside = 0;


	data.infoEngine.format = 0;
	data.fileEngine = sf_open(engineSoundFile.c_str(), SFM_READ, &data.infoEngine);
	if (sf_error(data.fileEngine) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on engineSoundFile " << engineSoundFile.c_str() << std::endl;
		return;
	}

	/* Open the soundfiles */
	data.infoWave.format = 0;

	data.fileWave = sf_open(waveSoundFile.c_str(), SFM_READ, &data.infoWave);
	if (sf_error(data.fileWave) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on waveSoundFile " << waveSoundFile.c_str() << std::endl;
		return;
	}

	data.infoHorn.format = 0;
	data.fileHorn = sf_open(hornSoundFile.c_str(), SFM_READ, &data.infoHorn);
	if (sf_error(data.fileHorn) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on hornSoundFile " << hornSoundFile.c_str() << " Error: " << sf_strerror(data.fileHorn) << std::endl;
		return;
	}

	data.infoAlarm.format = 0;
	data.fileAlarm = sf_open(alarmSoundFile.c_str(), SFM_READ, &data.infoAlarm);
	if (sf_error(data.fileAlarm) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on alarmSoundFile " << alarmSoundFile.c_str() << " Error: " << sf_strerror(data.fileAlarm) << std::endl;
		// Alarm is OPTIONAL: do NOT return here, or a missing alarm file silences
		// the entire sound system (engine, wave, horn too). The format check below
		// leaves alarmSoundLoaded == false, and the callback skips it. Only the
		// engine is mandatory (it defines the stream format).
	}

	// SOUND OUTSIDE AND INSIDE BOAT KYARA
	// These must be loaded unconditionally at the top level, NOT nested inside
	// the alarm-error block above (which was the original bug: they only loaded
	// when the alarm file was missing, so they never played during normal use).
	data.infoInside.format = 0;
	data.fileInside = sf_open(insideSoundFile.c_str(), SFM_READ, &data.infoInside);
	if (sf_error(data.fileInside) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on insideSoundFile " << insideSoundFile.c_str() << " Error: " << sf_strerror(data.fileInside) << std::endl;
	}
	else if (data.infoInside.channels != data.infoEngine.channels || data.infoInside.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent formats of inside and engine sounds, inside sound not loaded." << std::endl;
	}
	else {
		insideSoundLoaded = true;
	}

	data.infoOutside.format = 0;
	data.fileOutside = sf_open(outsideSoundFile.c_str(), SFM_READ, &data.infoOutside);
	if (sf_error(data.fileOutside) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on outsideSoundFile " << outsideSoundFile.c_str() << " Error: " << sf_strerror(data.fileOutside) << std::endl;
	}
	else if (data.infoOutside.channels != data.infoEngine.channels || data.infoOutside.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent formats of outside and engine sounds, outside sound not loaded." << std::endl;
	}
	else {
		outsideSoundLoaded = true;
	}

	//Check key parameters are the same
	if (data.infoWave.channels != data.infoEngine.channels || data.infoWave.samplerate != data.infoEngine.samplerate) {
		//Check wave vs engine
		std::cerr << "Inconsistent formats of wave and engine sounds, wave sound not loaded." << std::endl;
	}
	else {
		waveSoundLoaded = true;
	}

	if (data.infoHorn.channels != data.infoEngine.channels || data.infoHorn.samplerate != data.infoEngine.samplerate) {
		//Check wave vs engine
		std::cerr << "Inconsistent formats of horn and engine sounds, horn sound not loaded." << std::endl;
	}
	else {
		hornSoundLoaded = true;
	}

	if (data.infoAlarm.channels != data.infoEngine.channels || data.infoAlarm.samplerate != data.infoEngine.samplerate) {
		//Check alarm vs engine
		std::cerr << "Inconsistent formats of alarm and engine sounds, alarm sound not loaded." << std::endl;
	}
	else {
		alarmSoundLoaded = true;
	}
	//KYARA UPDATE -----------------------

	data.infoProxy.format = 0;
	data.fileProxy = sf_open(proxyAlarmSoundFile.c_str(), SFM_READ, &data.infoProxy);
	if (sf_error(data.fileProxy) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on proxyAlarmSoundFile " << proxyAlarmSoundFile.c_str() << " Error: " << sf_strerror(data.fileProxy) << std::endl;
	}
	else if (data.infoProxy.channels != data.infoEngine.channels || data.infoProxy.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent formats of proxy_alarm and engine sounds, proxy_alarm sound not loaded." << std::endl;
	}
	else {
		proxySoundLoaded = true;
	}
	//KYARA RADAR GUARD ALARM
	data.infoRadarAlarm.format = 0;
	data.fileRadarAlarm = sf_open(radarAlarmSoundFile.c_str(), SFM_READ, &data.infoRadarAlarm);
	if (sf_error(data.fileRadarAlarm) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on radarAlarmSoundFile " << radarAlarmSoundFile.c_str() << " Error: " << sf_strerror(data.fileRadarAlarm) << std::endl;
	}
	else if (data.infoRadarAlarm.channels != data.infoEngine.channels || data.infoRadarAlarm.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent formats of radar_alarm and engine sounds, radar_alarm sound not loaded." << std::endl;
	}
	else {
		radarAlarmSoundLoaded = true;
	}
	data.infoCollision.format = 0;
	data.fileCollision = sf_open(collisionSoundFile.c_str(), SFM_READ, &data.infoCollision);
	if (sf_error(data.fileCollision) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on collisionSoundFile " << collisionSoundFile.c_str() << " Error: " << sf_strerror(data.fileCollision) << std::endl;
	}
	else if (data.infoCollision.channels != data.infoEngine.channels || data.infoCollision.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent formats of collision and engine sounds, collision sound not loaded." << std::endl;
	}
	else {
		collisionSoundLoaded = true;
	}

	//KYARA SEAGULL: same load/validate pattern as the other one-shot (collision).
	//Seagull is OPTIONAL: a missing file just means the 'G' key plays nothing, it must
	//not affect any other sound channel.
	data.infoSeagull.format = 0;
	data.fileSeagull = sf_open(seagullSoundFile.c_str(), SFM_READ, &data.infoSeagull);
	if (sf_error(data.fileSeagull) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on seagullSoundFile " << seagullSoundFile.c_str() << " Error: " << sf_strerror(data.fileSeagull) << std::endl;
	}
	else if (data.infoSeagull.channels != data.infoEngine.channels || data.infoSeagull.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent formats of seagull and engine sounds, seagull sound not loaded." << std::endl;
	}
	else {
		seagullSoundLoaded = true;
	}
	// FIRE FEATURE — fireBurning (loop)
	data.infoFireBurning.format = 0;
	data.fileFireBurning = sf_open(fireBurningSoundFile.c_str(), SFM_READ, &data.infoFireBurning);
	if (sf_error(data.fileFireBurning) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on fireBurningSoundFile " << fireBurningSoundFile.c_str() << " Error: " << sf_strerror(data.fileFireBurning) << std::endl;
	}
	else if (data.infoFireBurning.channels != data.infoEngine.channels || data.infoFireBurning.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent formats of fire_burning and engine sounds, fire_burning sound not loaded." << std::endl;
	}
	else { fireBurningSoundLoaded = true; }

	// FIRE FEATURE — water (loop)
	data.infoWater.format = 0;
	data.fileWater = sf_open(waterSoundFile.c_str(), SFM_READ, &data.infoWater);
	if (sf_error(data.fileWater) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on waterSoundFile " << waterSoundFile.c_str() << " Error: " << sf_strerror(data.fileWater) << std::endl;
	}
	else if (data.infoWater.channels != data.infoEngine.channels || data.infoWater.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent formats of water and engine sounds, water sound not loaded." << std::endl;
	}
	else { waterSoundLoaded = true; }

	// FIRE FEATURE — fireAlarm (one-shot)
	data.infoFireAlarm.format = 0;
	data.fileFireAlarm = sf_open(fireAlarmSoundFile.c_str(), SFM_READ, &data.infoFireAlarm);
	if (sf_error(data.fileFireAlarm) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on fireAlarmSoundFile " << fireAlarmSoundFile.c_str() << " Error: " << sf_strerror(data.fileFireAlarm) << std::endl;
	}
	else if (data.infoFireAlarm.channels != data.infoEngine.channels || data.infoFireAlarm.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent formats of fire_alarm and engine sounds, fire_alarm sound not loaded." << std::endl;
	}
	else { fireAlarmSoundLoaded = true; }
	// FIRE — abandon (loop)
	data.infoAbandonAlarm.format = 0;
	data.fileAbandonAlarm = sf_open(abandonAlarmSoundFile.c_str(), SFM_READ, &data.infoAbandonAlarm);
	if (sf_error(data.fileAbandonAlarm) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on abandonAlarmSoundFile " << abandonAlarmSoundFile.c_str() << " Error: " << sf_strerror(data.fileAbandonAlarm) << std::endl;
	}
	else if (data.infoAbandonAlarm.channels != data.infoEngine.channels || data.infoAbandonAlarm.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent formats of abandon and engine sounds, abandon sound not loaded." << std::endl;
	}
	else { abandonAlarmSoundLoaded = true; }
	// FIRE — explosion (one-shot)
	data.infoExplosion.format = 0;
	data.fileExplosion = sf_open(explosionSoundFile.c_str(), SFM_READ, &data.infoExplosion);
	if (sf_error(data.fileExplosion) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on explosionSoundFile " << explosionSoundFile.c_str() << " Error: " << sf_strerror(data.fileExplosion) << std::endl;
	}
	else if (data.infoExplosion.channels != data.infoEngine.channels || data.infoExplosion.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent formats of explosion and engine sounds, explosion sound not loaded." << std::endl;
	}
	else { explosionSoundLoaded = true; }
	// FIRE — groan (loop)
	data.infoGroan.format = 0;
	data.fileGroan = sf_open(groanSoundFile.c_str(), SFM_READ, &data.infoGroan);
	if (sf_error(data.fileGroan) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on groanSoundFile " << groanSoundFile.c_str() << " Error: " << sf_strerror(data.fileGroan) << std::endl;
	}
	else if (data.infoGroan.channels != data.infoEngine.channels || data.infoGroan.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent formats of groan and engine sounds, groan sound not loaded." << std::endl;
	}
	else { groanSoundLoaded = true; }
	// FIRE — steam (loop)
	data.infoSteam.format = 0;
	data.fileSteam = sf_open(steamSoundFile.c_str(), SFM_READ, &data.infoSteam);
	if (sf_error(data.fileSteam) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on steamSoundFile " << steamSoundFile.c_str() << " Error: " << sf_strerror(data.fileSteam) << std::endl;
	}
	else if (data.infoSteam.channels != data.infoEngine.channels || data.infoSteam.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent formats of steam and engine sounds, steam sound not loaded." << std::endl;
	}
	else { steamSoundLoaded = true; }
	// FIRE — vhf (one-shot)
	data.infoVhf.format = 0;
	data.fileVhf = sf_open(vhfSoundFile.c_str(), SFM_READ, &data.infoVhf);
	if (sf_error(data.fileVhf) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on vhfSoundFile " << vhfSoundFile.c_str() << " Error: " << sf_strerror(data.fileVhf) << std::endl;
	}
	else if (data.infoVhf.channels != data.infoEngine.channels || data.infoVhf.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent formats of vhf and engine sounds, vhf sound not loaded." << std::endl;
	}
	else { vhfSoundLoaded = true; }
	// SAR: helicopter rotor loop (fixed path, so no load() signature change is needed)
	std::string heloSoundFile = "Sounds/helicopter.wav";
	data.infoHelo.format = 0;
	data.fileHelo = sf_open(heloSoundFile.c_str(), SFM_READ, &data.infoHelo);
	if (sf_error(data.fileHelo) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on heloSoundFile " << heloSoundFile.c_str() << " Error: " << sf_strerror(data.fileHelo) << std::endl;
	}
	else if (data.infoHelo.channels != data.infoEngine.channels || data.infoHelo.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent formats of helicopter and engine sounds, helicopter sound not loaded." << std::endl;
	}
	else { heloSoundLoaded = true; }
	//NAUTITECH HERO BOLT: optional one-shot, same validate pattern as the seagull.
	data.infoThunderbolt.format = 0;
	data.fileThunderbolt = sf_open(thunderboltSoundFile.c_str(), SFM_READ, &data.infoThunderbolt);
	if (sf_error(data.fileThunderbolt) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on thunderboltSoundFile " << thunderboltSoundFile.c_str() << " Error: " << sf_strerror(data.fileThunderbolt) << std::endl;
	}
	else if (data.infoThunderbolt.channels != data.infoEngine.channels || data.infoThunderbolt.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent formats of thunderbolt and engine sounds, thunderbolt sound not loaded." << std::endl;
	}
	else {
		thunderboltSoundLoaded = true;
	}
	//-------------------------------END OF UPDATE
	//KYARA CONTACT: OPTIONAL. A missing file just means no contact sound; it must not
	//affect any other channel, so we never return early here.
	data.infoContact.format = 0;
	data.fileContact = sf_open(contactSoundFile.c_str(), SFM_READ, &data.infoContact);
	if (sf_error(data.fileContact) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on contactSoundFile " << contactSoundFile.c_str() << " Error: " << sf_strerror(data.fileContact) << std::endl;
	}
	else if (data.infoContact.channels != data.infoEngine.channels || data.infoContact.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent formats of contact and engine sounds, contact sound not loaded." << std::endl;
	}
	else {
		contactSoundLoaded = true;
	}
	// KYARA WEATHER SFX: all optional; a missing file just disables that one channel.
	data.infoRain.format = 0;
	data.fileRain = sf_open(rainSoundFile.c_str(), SFM_READ, &data.infoRain);
	if (sf_error(data.fileRain) == SF_ERR_NO_ERROR &&
		data.infoRain.channels == data.infoEngine.channels && data.infoRain.samplerate == data.infoEngine.samplerate) {
		rainSoundLoaded = true;
	}
	else { std::cerr << "rain sound not loaded (missing or format mismatch): " << rainSoundFile << std::endl; }

	data.infoStorm.format = 0;
	data.fileStorm = sf_open(stormSoundFile.c_str(), SFM_READ, &data.infoStorm);
	if (sf_error(data.fileStorm) == SF_ERR_NO_ERROR &&
		data.infoStorm.channels == data.infoEngine.channels && data.infoStorm.samplerate == data.infoEngine.samplerate) {
		stormSoundLoaded = true;
	}
	else { std::cerr << "storm sound not loaded (missing or format mismatch): " << stormSoundFile << std::endl; }

	data.infoThunder.format = 0;
	data.fileThunder = sf_open(thunderSoundFile.c_str(), SFM_READ, &data.infoThunder);
	if (sf_error(data.fileThunder) == SF_ERR_NO_ERROR &&
		data.infoThunder.channels == data.infoEngine.channels && data.infoThunder.samplerate == data.infoEngine.samplerate) {
		thunderSoundLoaded = true;
	}
	else { std::cerr << "thunder sound not loaded (missing or format mismatch): " << thunderSoundFile << std::endl; }
	/* Open PaStream with values read from the file - all should be same, so any can be used*/
	portAudioError = Pa_OpenDefaultStream(&stream
		, 0                     /* no input */
		, data.infoEngine.channels         /* stereo out */
		, paFloat32             /* floating point */
		, data.infoEngine.samplerate
		, FRAMES_PER_BUFFER
		, callback
		, &data);        /* our sndfile data struct */
	if (portAudioError != paNoError)
	{
		std::cerr << "Pa_OpenDefaultStream failed." << std::endl;
		std::cerr << "Error: " << Pa_GetErrorText(portAudioError) << std::endl;
		return;
	}

	//The sonar's loudspeaker sounds, made here (no files). As a search sonar's audio (and the reference
	//recording they were matched to): a pure tone of about 1 kHz, struck and fading slowly, the water's
	//reverberation a soft hiss under its tail and a light repeating echo (every 0.13 s, each repeat
	//8 dB down); every echo is that same tone, weaker.
	//0 the transmission's ping: 1030 Hz, no harmonics, up in 40 ms, -10 dB at 0.7 s, -20 dB at 1.2 s,
	//  about 2.5 s in all; 1 a hull or a wreck: the same tone, shorter and clean; 2 a fish school (many
	//  fish at slightly different ranges and speeds): the tone softer, wavering and smeared, longer;
	//3 (not played) a soft hiss; 4 the fish alarm (an electronic double beep).
	{
		const double rate = (double)data.infoEngine.samplerate;
		const double pi = 3.14159265358979;
		const double TONE = 1030.0;
		std::mt19937 rng(7);
		std::uniform_real_distribution<double> uni(-1.0, 1.0);
		//A pure tone: rise in 'attack', fall as exp(-t / tau), a slow slight waver (the water)
		auto pure = [&](double seconds, double f, double attack, double tau, double waver, double startPhase) {
			const size_t n = (size_t)(seconds * rate);
			std::vector<float> out(n, 0.0f);
			double phase = startPhase;
			for (size_t i = 0; i < n; i++) {
				const double t = i / rate;
				phase += 2.0 * pi * f / rate;
				const double env = std::min(t / attack, 1.0) * std::exp(-t / tau) * (1.0 - waver * (0.5 + 0.5 * std::sin(2.0 * pi * 3.1 * t)));
				out[i] = (float)(std::sin(phase) * env);
			}
			return out;
		};
		//Diffuse reverberation: many small late copies, fading, so the tail is smooth (no distinct repeats)
		auto diffuse = [&](std::vector<float>& x, int taps, double maxDelay, float level) {
			std::vector<float> dry(x);
			for (int d = 0; d < taps; d++) {
				const double delay = 0.02 + maxDelay * (0.5 + 0.5 * uni(rng));
				const float g = level * (float)std::exp(-delay / (0.6 * maxDelay)) * (float)(0.6 + 0.4 * (0.5 + 0.5 * uni(rng)));
				const size_t off = (size_t)(delay * rate);
				for (size_t i = off; i < x.size(); i++) { x[i] += g * dry[i - off]; }
			}
		};
		//The water's hiss under a sound: soft, dull, fading with it
		auto hiss = [&](std::vector<float>& x, float level, double tau) {
			double state = 0.0;
			for (size_t i = 0; i < x.size(); i++) {
				state = 0.92 * state + 0.08 * uni(rng);
				const double t = i / rate;
				x[i] += (float)(level * state * std::min(t / 0.05, 1.0) * std::exp(-t / tau));
			}
		};
		//A light repeating echo, as in the reference recording: a repeat every 'delay', each 'g' of the
		//one before, and each a little off pitch (+-0.8 %, as sound back from moving water), so that it
		//beats against the tone still sounding and is heard as an echo rather than merging into it
		auto echoes = [&](std::vector<float>& x, double delay, float g) {
			const std::vector<float> dry(x);
			float gain = 1.0f;
			for (int r = 1; r <= 5; r++) {
				gain *= g;
				const double ratio = 1.0 + ((r % 2) ? 0.008 : -0.008);
				const size_t off = (size_t)(r * delay * rate);
				for (size_t i = off; i < x.size(); i++) {
					const double p = (double)(i - off) * ratio;
					const size_t i0 = (size_t)p;
					if (i0 + 1 >= dry.size()) { break; }
					x[i] += gain * (float)(dry[i0] + (dry[i0 + 1] - dry[i0]) * (p - (double)i0));
				}
			}
		};
		auto normalise = [&](std::vector<float>& x, float peakWanted) {
			float peak = 0.0f;
			for (size_t i = 0; i < x.size(); i++) { peak = std::max(peak, std::fabs(x[i])); }
			if (peak > 0.0f) { for (size_t i = 0; i < x.size(); i++) { x[i] *= peakWanted / peak; } }
		};
		sonarSounds[0] = pure(2.6, TONE, 0.04, 0.52, 0.08, 0.0);
		diffuse(sonarSounds[0], 10, 0.25, 0.10f);
		echoes(sonarSounds[0], 0.13, 0.4f);
		hiss(sonarSounds[0], 0.06f, 0.7);
		normalise(sonarSounds[0], 0.8f);
		sonarSounds[1] = pure(1.4, TONE, 0.015, 0.28, 0.0, 0.0);
		diffuse(sonarSounds[1], 6, 0.15, 0.06f);
		echoes(sonarSounds[1], 0.13, 0.35f);
		normalise(sonarSounds[1], 0.8f);
		{
			//The school: the tone from several groups of fish, each a little higher or lower (their own
			//movement) and a few milliseconds later (their range), added up - they beat against each
			//other, so the echo wavers; a soft start, a long tail
			const int groups = 6;
			sonarSounds[2].assign((size_t)(2.0 * rate), 0.0f);
			for (int g = 0; g < groups; g++) {
				const double detune = 1.0 + 0.008 * uni(rng);
				const double delay = 0.004 + 0.04 * (0.5 + 0.5 * uni(rng));
				const double amp = 0.6 + 0.4 * (0.5 + 0.5 * uni(rng));
				const std::vector<float> one = pure(2.0 - delay, TONE * detune, 0.04, 0.4, 0.0, 2.0 * pi * (0.5 + 0.5 * uni(rng)));
				const size_t off = (size_t)(delay * rate);
				for (size_t i = 0; i < one.size() && off + i < sonarSounds[2].size(); i++) { sonarSounds[2][off + i] += (float)amp * one[i]; }
			}
			diffuse(sonarSounds[2], 6, 0.15, 0.08f);
			echoes(sonarSounds[2], 0.13, 0.3f);
			normalise(sonarSounds[2], 0.7f);
		}
		sonarSounds[3].assign((size_t)(1.0 * rate), 0.0f);
		hiss(sonarSounds[3], 0.3f, 0.4);
		{
			const size_t n = (size_t)(0.42 * rate);
			sonarSounds[4].assign(n, 0.0f);
			for (size_t i = 0; i < n; i++) {
				const double t = i / rate;
				const bool on = (t < 0.13) || (t > 0.21 && t < 0.34);
				if (!on) { continue; }
				const double s = std::sin(2.0 * pi * 2400.0 * t);
				sonarSounds[4][i] = (float)(0.6 * (s + 0.3 * std::sin(6.0 * pi * 2400.0 * t)));   //(a buzzer's edge)
			}
		}
	}

	soundLoaded = true; // All OK if we've got here
	std::cout << "Sound::load succeeded" << std::endl;
}

void Sound::StartSound() {
	/* Start the stream */

	if (soundLoaded) {
		portAudioError = Pa_StartStream(stream);
		if (portAudioError != paNoError)
		{
			std::cerr << "Problem opening starting Stream" << std::endl;
			std::cerr << "Error: " << Pa_GetErrorText(portAudioError) << std::endl;
		}
	}

}

void Sound::setVolumeWave(float vol) {
	if (vol >= 0 && vol <= 1.0) {
		Sound::waveVolume = vol;
	}
}

void Sound::setVolumeEngine(float vol) {
	if (vol >= 0 && vol <= 1.0) {
		Sound::engineVolume = vol;
	}
}

void Sound::setVolumeHorn(float vol) {
	if (vol >= 0 && vol <= 1.0) {
		Sound::hornVolume = vol;
	}
}

void Sound::setVolumeAlarm(float vol) {
	if (vol >= 0 && vol <= 1.0) {
		Sound::alarmVolume = vol;
	}
}

float Sound::getVolumeWave() const {
	return Sound::waveVolume;
}

float Sound::getVolumeEngine() const {
	return Sound::engineVolume;
}

float Sound::getVolumeHorn() const {
	return Sound::hornVolume;
}

float Sound::getVolumeAlarm() const {
	return Sound::alarmVolume;
}
//STORM
void  Sound::setVolumeRain(float vol) { if (vol >= 0 && vol <= 1.0) Sound::rainVolume = vol; }
float Sound::getVolumeRain() const { return Sound::rainVolume; }
void  Sound::setVolumeStorm(float vol) { if (vol >= 0 && vol <= 1.0) Sound::stormVolume = vol; }
float Sound::getVolumeStorm() const { return Sound::stormVolume; }
void  Sound::setVolumeThunder(float vol) { if (vol >= 0 && vol <= 6.0) Sound::thunderVolume = vol; }
float Sound::getVolumeThunder() const { return Sound::thunderVolume; }
double Sound::getThunderPositionSeconds() const { return Sound::thunderPosSeconds; }

//KYARA ENGINE PITCH: playback-rate multiplier for the engine sound. Clamped to keep the
//pitch shift audible-but-realistic rather than letting an extreme input value turn the
//engine into a chipmunk (too high) or a near-silent rumble (too low, since very slow
//playback also drops the engine's volume of higher harmonics out of the audible mix).
//0.5 = an octave down, 2.0 = an octave up; that's a wide enough range for any throttle
//mapping while staying away from degenerate/aliased extremes.
void Sound::setPitchEngine(float pitch) {
	if (pitch < 0.5f) { pitch = 0.5f; }
	if (pitch > 2.0f) { pitch = 2.0f; }
	Sound::enginePitch = pitch;
}

float Sound::getPitchEngine() const {
	return Sound::enginePitch;
}
//KYARA COLLISIOM AND PROXY-----------
void Sound::setVolumeProxyAlarm(float vol) {
	if (vol >= 0 && vol <= 6.0) {
		Sound::proxyVolume = vol;
	}

}

void Sound::setVolumeRadarAlarm(float vol) {
	if (soundLoaded) {
		Sound::radarAlarmVolume = vol;
	}
}

void Sound::setVolumeCollision(float vol) {
	if (vol >= 0 && vol <= 6.0) {
		Sound::collisionVolume = vol;
	}
}

float Sound::getVolumeProxyAlarm() const {
	return Sound::proxyVolume;
}

float Sound::getVolumeCollision() const {
	return Sound::collisionVolume;
}
float Sound::getVolumeRadarAlarm() const {
	return Sound::radarAlarmVolume;
}

//KYARA: arm the collision one-shot. The audio callback picks this up on its next
//buffer, seeks the collision file to the start, and plays it through exactly once.
void Sound::triggerCollision() {
	Sound::collisionTriggered = true;
}
//------------------END OF UPDATE

//KYARA SEAGULL: volume control and one-shot trigger for the 'G' key sound. Same pattern
//as triggerCollision() above - arms a flag, the audio callback seeks to the start and
//plays through exactly once, no looping.
void Sound::setVolumeSeagull(float vol) {
	if (vol >= 0 && vol <= 1.0) {
		Sound::seagullVolume = vol;
	}
}

float Sound::getVolumeSeagull() const {
	return Sound::seagullVolume;
}

void Sound::triggerSeagull() {
	Sound::seagullTriggered = true;
}
// FIRE FEATURE
void Sound::setVolumeFireBurning(float vol) { if (vol >= 0 && vol <= 1.0f) Sound::fireBurningVolume = vol; }
float Sound::getVolumeFireBurning() const { return Sound::fireBurningVolume; }
void Sound::setVolumeWater(float vol) { if (vol >= 0 && vol <= 1.0f) Sound::waterVolume = vol; }
float Sound::getVolumeWater() const { return Sound::waterVolume; }
void Sound::setVolumeFireAlarm(float vol) { if (vol >= 0 && vol <= 1.0f) Sound::fireAlarmVolume = vol; }
float Sound::getVolumeFireAlarm() const { return Sound::fireAlarmVolume; }
void Sound::triggerFireAlarm() { Sound::fireAlarmTriggered = true; }
void Sound::triggerExplosion() { Sound::explosionTriggered = true; }
void Sound::setVolumeVhf(float vol) { if (vol >= 0 && vol <= 1.0f) Sound::vhfVolume = vol; }
float Sound::getVolumeVhf() const { return Sound::vhfVolume; }
void Sound::setVolumeHelo(float vol) { if (vol >= 0 && vol <= 1.0f) Sound::heloVolume = vol; }
float Sound::getVolumeHelo() const { return Sound::heloVolume; }
void Sound::setVolumeAbandonAlarm(float vol) { if (vol >= 0 && vol <= 1.0f) Sound::abandonAlarmVolume = vol; }
float Sound::getVolumeAbandonAlarm() const { return Sound::abandonAlarmVolume; }
void Sound::setVolumeGroan(float vol) { if (vol >= 0 && vol <= 1.0f) Sound::groanVolume = vol; }
float Sound::getVolumeGroan() const { return Sound::groanVolume; }
void Sound::setVolumeSteam(float vol) { if (vol >= 0 && vol <= 1.0f) Sound::steamVolume = vol; }
float Sound::getVolumeSteam() const { return Sound::steamVolume; }
void Sound::triggerThunderbolt() {
	Sound::thunderboltTriggered = true;
}

//SOUND OUTSIDE AND INSIDE BOAT KYARA
void Sound::setVolumeInside(float vol) {
	if (vol >= 0 && vol <= 1.0) {
		Sound::insideVolume = vol;
	}
}

void Sound::setVolumeOutside(float vol) {
	if (vol >= 0 && vol <= 1.0) {
		Sound::outsideVolume = vol;
	}
}

//KYARA CONTACT
void Sound::setVolumeContact(float vol) {
	if (vol >= 0 && vol <= 6.0) {
		Sound::contactVolume = vol;
	}
}

float Sound::getVolumeContact() const {
	return Sound::contactVolume;
}

void Sound::triggerContact() {
	Sound::contactTriggered = true;
}

//KYARA SLAM -------------------------------------------------------------------------------------
//Loaded after load(), so the (already very long) load() signature stays as it is. All three files
//are optional: a missing one just means the callback falls back to another level, and if none load
//the ship simply makes no slam noise.
void Sound::loadSlamSound(std::string slamFile) {

	if (!soundLoaded) { return; } //no audio stream at all

	slamSoundLoaded = false;
	data.fileSlam = 0;
	if (slamFile.empty()) { return; }

	data.infoSlam.format = 0;
	data.fileSlam = sf_open(slamFile.c_str(), SFM_READ, &data.infoSlam);
	if (sf_error(data.fileSlam) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on slam sound " << slamFile << ": " << sf_strerror(data.fileSlam) << std::endl;
	}
	else if (data.infoSlam.channels != data.infoEngine.channels ||
			 data.infoSlam.samplerate != data.infoEngine.samplerate) {
		//Same rule as every other channel here: the mixer assumes one common format.
		std::cerr << "Inconsistent format of slam and engine sounds, slam sound not loaded: " << slamFile << std::endl;
	}
	else {
		slamSoundLoaded = true;
	}
}

bool Sound::hasSlamSound() const {
	return slamSoundLoaded;
}

void Sound::loadCpaAlarmSound(std::string cpaAlarmFile) {
	if (!soundLoaded) { return; } //no audio stream at all
	cpaAlarmSoundLoaded = false;
	data.fileCpaAlarm = 0;
	if (cpaAlarmFile.empty()) { return; }
	data.infoCpaAlarm.format = 0;
	data.fileCpaAlarm = sf_open(cpaAlarmFile.c_str(), SFM_READ, &data.infoCpaAlarm);
	if (sf_error(data.fileCpaAlarm) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on CPA alarm sound " << cpaAlarmFile << ": " << sf_strerror(data.fileCpaAlarm) << std::endl;
	}
	else if (data.infoCpaAlarm.channels != data.infoEngine.channels ||
			 data.infoCpaAlarm.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent format of CPA alarm and engine sounds, CPA alarm sound not loaded: " << cpaAlarmFile << std::endl;
	}
	else {
		cpaAlarmSoundLoaded = true;
	}
}

void Sound::setVolumeCpaAlarm(float vol) {
	if (vol >= 0.0f && vol <= 1.0f) {
		Sound::cpaAlarmVolume = vol;
	}
}

void Sound::loadDepthAlarmSound(std::string depthAlarmFile) {
	if (!soundLoaded) { return; } //no audio stream at all
	depthAlarmSoundLoaded = false;
	data.fileDepthAlarm = 0;
	if (depthAlarmFile.empty()) { return; }
	data.infoDepthAlarm.format = 0;
	data.fileDepthAlarm = sf_open(depthAlarmFile.c_str(), SFM_READ, &data.infoDepthAlarm);
	if (sf_error(data.fileDepthAlarm) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on depth alarm sound " << depthAlarmFile << ": " << sf_strerror(data.fileDepthAlarm) << std::endl;
	}
	else if (data.infoDepthAlarm.channels != data.infoEngine.channels ||
			 data.infoDepthAlarm.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent format of depth alarm and engine sounds, depth alarm sound not loaded: " << depthAlarmFile << std::endl;
	}
	else {
		depthAlarmSoundLoaded = true;
	}
}

void Sound::setVolumeDepthAlarm(float vol) {
	if (vol >= 0.0f && vol <= 1.0f) {
		Sound::depthAlarmVolume = vol;
	}
}

void Sound::loadFailureAlarmSound(std::string failureAlarmFile) {
	if (!soundLoaded) { return; } //no audio stream at all
	failureAlarmSoundLoaded = false;
	data.fileFailureAlarm = 0;
	if (failureAlarmFile.empty()) { return; }
	data.infoFailureAlarm.format = 0;
	data.fileFailureAlarm = sf_open(failureAlarmFile.c_str(), SFM_READ, &data.infoFailureAlarm);
	if (sf_error(data.fileFailureAlarm) != SF_ERR_NO_ERROR) {
		std::cerr << "sf_error on failure alarm sound " << failureAlarmFile << ": " << sf_strerror(data.fileFailureAlarm) << std::endl;
	}
	else if (data.infoFailureAlarm.channels != data.infoEngine.channels ||
			 data.infoFailureAlarm.samplerate != data.infoEngine.samplerate) {
		std::cerr << "Inconsistent format of failure alarm and engine sounds, failure alarm sound not loaded: " << failureAlarmFile << std::endl;
	}
	else {
		failureAlarmSoundLoaded = true;
	}
}

void Sound::setVolumeFailureAlarm(float vol) {
	if (vol >= 0.0f && vol <= 1.0f) {
		Sound::failureAlarmVolume = vol;
	}
}

void Sound::setVolumeSlam(float vol) {
	if (vol >= 0.0f && vol <= 1.0f) {
		Sound::slamVolume = vol;
	}
}

float Sound::getVolumeSlam() const {
	return Sound::slamVolume;
}

void Sound::setVolumeSonar(float vol) {
	if (vol >= 0.0f && vol <= 1.0f) {
		Sound::sonarVolume = vol;
	}
}

float Sound::getVolumeSonar() const {
	return Sound::sonarVolume;
}

void Sound::triggerSonar(float gain, float pitch, int kind) {
	if (!soundLoaded) { return; }
	const unsigned w = sonarRequestsWritten.load(std::memory_order_relaxed);
	sonarRequests[w % 16].gain = std::max(0.0f, std::min(gain, 1.0f));
	sonarRequests[w % 16].pitch = std::max(0.25f, std::min(pitch, 4.0f));
	sonarRequests[w % 16].kind = kind;
	sonarRequestsWritten.store(w + 1, std::memory_order_release);
}

void Sound::triggerSlam(float gain, float pitch) {
	if (gain < 0.0f) { gain = 0.0f; }
	if (gain > 1.0f) { gain = 1.0f; }
	if (pitch < 0.25f) { pitch = 0.25f; }
	if (pitch > 4.0f) { pitch = 4.0f; }
	//Write the shape of the impact first, then the trigger flag: the callback reads the flag
	//last, so it can never pick up a half-updated set of values.
	Sound::slamGain = gain;
	Sound::slamPitch = pitch;
	Sound::slamTriggered = true;
}

Sound::~Sound() {

	if (soundLoaded && stream) {
		portAudioError = Pa_CloseStream(stream);
	}

	portAudioError = Pa_Terminate();

	/* Close the soundfile */
	if (data.fileWave) { sf_close(data.fileWave); }
	if (data.fileEngine) { sf_close(data.fileEngine); }
	if (data.fileHorn) { sf_close(data.fileHorn); }
	if (data.fileAlarm) { sf_close(data.fileAlarm); }
	if (data.fileFireBurning) { sf_close(data.fileFireBurning); } // FIRE FEATURE
	if (data.fileWater) { sf_close(data.fileWater); }
	if (data.fileFireAlarm) { sf_close(data.fileFireAlarm); }
	if (data.fileAbandonAlarm) { sf_close(data.fileAbandonAlarm); }
	if (data.fileExplosion) { sf_close(data.fileExplosion); }
	if (data.fileGroan) { sf_close(data.fileGroan); }
	if (data.fileSteam) { sf_close(data.fileSteam); }
	if (data.fileVhf) { sf_close(data.fileVhf); }
	if (data.fileHelo) { sf_close(data.fileHelo); }
	// kyara update 
	//KYARA: close proxy and collision here, not in the volume setter
	if (data.fileProxy) { sf_close(data.fileProxy); }
	if (data.fileRadarAlarm) { sf_close(data.fileRadarAlarm); }
	if (data.fileCollision) { sf_close(data.fileCollision); }
	//KYARA SEAGULL
	if (data.fileSeagull) { sf_close(data.fileSeagull); }
	if (data.fileThunderbolt) { sf_close(data.fileThunderbolt); }
	if (data.fileContact) { sf_close(data.fileContact); }
	//KYARA SLAM
	if (data.fileSlam) { sf_close(data.fileSlam); }
	if (data.fileCpaAlarm) { sf_close(data.fileCpaAlarm); }
	if (data.fileDepthAlarm) { sf_close(data.fileDepthAlarm); }
	if (data.fileFailureAlarm) { sf_close(data.fileFailureAlarm); }
	//CAMERA CHANGE SOUND CHANGE
	if (data.fileInside) { sf_close(data.fileInside); }
	if (data.fileOutside) { sf_close(data.fileOutside); }
	if (data.fileRain) { sf_close(data.fileRain); }
	if (data.fileStorm) { sf_close(data.fileStorm); }
	if (data.fileThunder) { sf_close(data.fileThunder); }
}

#endif // WITH_SOUND