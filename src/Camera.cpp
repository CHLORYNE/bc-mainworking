/*   Bridge Command 5.0 Ship Simulator
     Copyright (C) 2014 James Packer

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY Or FITNESS For A PARTICULAR PURPOSE.  See the
     GNU General Public License For more details.

     You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

#include "Camera.hpp"
#include "Constants.hpp"

#include <cmath>


//using namespace irr;

Camera::Camera()
{
    //KYARA FEUX EDIT
    orbiting = false;
    orbitHeading = 0.0f;
    orbitYaw = -60.0f;
    orbitPitch = 15.0f;
    orbitRadius = 40.0f;
    orbitMinPitch = -10.0f;
    orbitMinHeight = -1e9f;
}

Camera::~Camera()
{
    //dtor
}


void Camera::load(irr::scene::ISceneManager* smgr, irr::ILogger* logger, irr::scene::ISceneNode* parent, std::vector<irr::core::vector3df> views, std::vector<bool> isHighView, irr::f32 hFOV, irr::f32 lookAngle, irr::f32 angleCorrection)
{
    this->hFOV = hFOV;
    camera = smgr->addCameraSceneNode(0, irr::core::vector3df(0,0,0), irr::core::vector3df(0,0,1));

    this->parent = parent;
    this->views = views;
    this->isHighView = isHighView;
    currentView = 0;
    this->lookAngle = lookAngle;
    minLookUpAngle = -85;
    maxLookUpAngle = 85;
    lookUpAngle = 0;
    this->angleCorrection = angleCorrection;

    this->logger = logger;

    verticalPanSpeed = 0;
    horizontalPanSpeed = 0;

    isHighViewActive = false;
    previousLookAngle = lookAngle;
    previousLookUpAngle = lookUpAngle;

    frozen = false;

    sideViewVector = irr::core::vector3df(1,0,0);
}

irr::scene::ISceneNode* Camera::getSceneNode() const
{
    return camera;
}

//Return the position of the camera, including any active VR camera offset
irr::core::vector3df Camera::getPosition() const
{
    camera->updateAbsolutePosition();//ToDo: This may be needed, but seems odd that it's required
    return camera->getAbsolutePosition();
}

// Return the position of the base camera, without any VR camera offset
irr::core::vector3df Camera::getBasePosition() const
{
    // Uses current parentPosition and parentAngles
    irr::core::vector3df offsetTransformed;
    parentAngles.transformVect(offsetTransformed,views[currentView]);
    irr::core::vector3df cameraPosition = parentPosition + offsetTransformed;
    return cameraPosition;
}

irr::core::matrix4 Camera::getBaseRotation() const
{
    return parentAngles;
}

void Camera::lookUp()
{
    lookUpAngle++;
    if (lookUpAngle>maxLookUpAngle)
    {
        lookUpAngle=maxLookUpAngle;
    }
}

void Camera::lookDown()
{
    lookUpAngle--;
    if (lookUpAngle<minLookUpAngle)
    {
        lookUpAngle=minLookUpAngle;
    }
}

void Camera::setPanSpeed(irr::f32 horizontalPanSpeed){
    this->horizontalPanSpeed = horizontalPanSpeed;
}

void Camera::setVerticalPanSpeed(irr::f32 verticalPanSpeed){
    this->verticalPanSpeed = verticalPanSpeed;
}

void Camera::setLookUp(irr::f32 angle)
{
	lookUpAngle = angle;
}

void Camera::lookLeft()
{
    lookAngle--;
    while (lookAngle<0)
    {
        lookAngle+=360;
    }
}

void Camera::lookRight()
{
    lookAngle++;
    while (lookAngle>=360)
    {
        lookAngle-=360;
    }
}

void Camera::lookChange(irr::f32 deltaX, irr::f32 deltaY) //Change as a proportion of screen width
{
    lookAngle -= deltaX*hFOV*irr::core::RADTODEG;
    lookUpAngle += deltaY*hFOV*irr::core::RADTODEG; //hFOV for this, as both are scaled by screen width
    while (lookAngle<0)
    {
        lookAngle+=360;
    }
    while (lookAngle>=360)
    {
        lookAngle-=360;
    }
    if (lookUpAngle>maxLookUpAngle)
    {
        lookUpAngle=maxLookUpAngle;
    }
    if (lookUpAngle<minLookUpAngle)
    {
        lookUpAngle=minLookUpAngle;
    }
}

void Camera::lookStepLeft()
{
    lookAngle -= hFOV*irr::core::RADTODEG;
    while (lookAngle<0)
    {
        lookAngle+=360;
    }
}

void Camera::lookStepRight()
{
    lookAngle += hFOV*irr::core::RADTODEG;
    while (lookAngle>=360)
    {
        lookAngle-=360;
    }
}

void Camera::lookAhead()
{
    lookAngle = 0;
    lookUpAngle = 0;
}

void Camera::lookAstern()
{
    lookAngle = 180;
    lookUpAngle = 0;
}

void Camera::lookPort()
{
    lookAngle = 270;
    lookUpAngle = 0;
}

void Camera::lookStbd()
{
    lookAngle = 90;
    lookUpAngle = 0;
}

irr::f32 Camera::getLook() const
{
    return lookAngle;
}

irr::f32 Camera::getLookUp() const
{
    return lookUpAngle;
}

irr::core::vector3df Camera::getForwardVector() const
{
    return frv;
}

void Camera::moveForwards()
{
    irr::core::vector3df frvLocal(1.0f*std::sin(irr::core::DEGTORAD*(lookAngle-angleCorrection))*std::cos(irr::core::DEGTORAD*lookUpAngle), 1.0f*std::sin(irr::core::DEGTORAD*lookUpAngle), 1.0f*std::cos(irr::core::DEGTORAD*(lookAngle-angleCorrection))*std::cos(irr::core::DEGTORAD*lookUpAngle));
    views[currentView] += 0.05 * frvLocal;

    //For displaying position
    std::string cameraPositionText = "Camera: (";
    cameraPositionText.append(irr::core::stringc(views[currentView].X).c_str());
    cameraPositionText.append(",");
    cameraPositionText.append(irr::core::stringc(views[currentView].Y).c_str());
    cameraPositionText.append(",");
    cameraPositionText.append(irr::core::stringc(views[currentView].Z).c_str());
    cameraPositionText.append(")");
    logger->log(cameraPositionText.c_str());
}

void Camera::moveBackwards()
{
    irr::core::vector3df frvLocal(1.0f*std::sin(irr::core::DEGTORAD*(lookAngle-angleCorrection))*std::cos(irr::core::DEGTORAD*lookUpAngle), 1.0f*std::sin(irr::core::DEGTORAD*lookUpAngle), 1.0f*std::cos(irr::core::DEGTORAD*(lookAngle-angleCorrection))*std::cos(irr::core::DEGTORAD*lookUpAngle));
    views[currentView] -= 0.05 * frvLocal;

    //For displaying position
    std::string cameraPositionText = "Camera: (";
    cameraPositionText.append(irr::core::stringc(views[currentView].X).c_str());
    cameraPositionText.append(",");
    cameraPositionText.append(irr::core::stringc(views[currentView].Y).c_str());
    cameraPositionText.append(",");
    cameraPositionText.append(irr::core::stringc(views[currentView].Z).c_str());
    cameraPositionText.append(")");
    logger->log(cameraPositionText.c_str());
}

void Camera::highView(bool highViewRequired)
{
    if (isHighViewActive != highViewRequired) {
        if (!highViewRequired) {
            lookAngle = previousLookAngle;
            lookUpAngle = previousLookUpAngle;
            isHighViewActive = false;
        } else {
            previousLookAngle = lookAngle;
            previousLookUpAngle = lookUpAngle;
            lookAngle = 0;
            lookUpAngle = -89.99; //Almost straight down. Avoid -90 as this gives an odd rotation effect (gymbal lock?)
            isHighViewActive = true;
        }
    }
}

void Camera::changeView()
{
    currentView++;
    if (currentView==views.size()) {
        currentView = 0;
    }

    if (currentView<isHighView.size()) {
        if (isHighView[currentView]) {
            highView(true);
        } else {
            highView(false);
        }
    }

}

void Camera::setView(irr::u32 view) {
    if (view<views.size()) {
        currentView = view;
    }

    if (currentView<isHighView.size()) {
        if (isHighView[currentView]) {
            highView(true);
        } else {
            highView(false);
        }
    }

}

irr::u32 Camera::getViewCount() const
{
    return (irr::u32)views.size();
}

irr::u32 Camera::getView() const
{
    return currentView;
}

void Camera::setHFOV(irr::f32 hFOV)
{
    this->hFOV=hFOV;
    irr::f32 aspect=camera->getAspectRatio();

    irr::f32 vFOV = 2*atan(tan(hFOV/2)/aspect); //Calculate vertical field of view angle from horizontal one
    camera->setFOV(vFOV);
}

void Camera::updateViewport(irr::f32 aspect)
{
    camera->setAspectRatio(aspect);
    irr::f32 vFOV = 2*atan(tan(hFOV/2)/aspect); //Calculate vertical field of view angle from horizontal one
    camera->setFOV(vFOV);
}

void Camera::setActive()
{
    camera->getSceneManager()->setActiveCamera(camera);
}

void Camera::setNearValue(irr::f32 zn)
{
    camera->setNearValue(zn);
}

void Camera::setFarValue(irr::f32 zf)
{
    camera->setFarValue(zf);
}

void Camera::setFrozen(bool frozen)
{
    this->frozen = frozen;
}

void Camera::toggleFrozen()
{
    frozen = !frozen;
}

void Camera::applyOffset(irr::f32 deltaX, irr::f32 deltaY, irr::f32 deltaZ)
{
    if (frozen) {
        // Only applicable in frozen mode, otherwise update will be
        // handled when own ship position offset is applied
        parentPosition.X += deltaX;
        parentPosition.Y += deltaY;
        parentPosition.Z += deltaZ;
    }
}

//KYARA FEUX EDIT -----------------------------------------------------------------------------------
void Camera::setOrbit(bool on, irr::f32 radiusMetres)
{
    orbiting = on;
    if (on) {
        orbitYaw = -60.0f;   //port bow: the red sidelight and the mast in one view
        orbitPitch = 15.0f;
        orbitRadius = radiusMetres;
        orbitMinPitch = -10.0f;
        orbitMinHeight = -1e9f;
    }
}

void Camera::setOrbitMinHeight(irr::f32 minY)
{
    orbitMinHeight = minY;
}

irr::core::vector3df Camera::getOrbitPosition() const
{
    irr::core::vector3df pos, fwd;
    orbitPose(pos, fwd);
    return pos;
}

void Camera::setOrbitMinPitch(irr::f32 minPitchDeg)
{
    orbitMinPitch = minPitchDeg;
    if (orbitPitch < orbitMinPitch) { orbitPitch = orbitMinPitch; }
}

bool Camera::isOrbiting() const
{
    return orbiting;
}

void Camera::setOrbitCentre(irr::core::vector3df centre, irr::f32 vesselHeadingDeg)
{
    orbitCentre = centre;
    orbitHeading = vesselHeadingDeg;
}

void Camera::orbitBy(irr::f32 dYawDeg, irr::f32 dPitchDeg, irr::f32 zoomFactor)
{
    orbitYaw += dYawDeg;
    while (orbitYaw >= 360.0f) { orbitYaw -= 360.0f; }
    while (orbitYaw < 0.0f) { orbitYaw += 360.0f; }
    orbitPitch += dPitchDeg;
    if (orbitPitch > 85.0f) { orbitPitch = 85.0f; }
    if (orbitPitch < orbitMinPitch) { orbitPitch = orbitMinPitch; } //lamps: a little below the lamp, never under the sea
    orbitRadius *= zoomFactor;
    if (orbitRadius < 2.0f) { orbitRadius = 2.0f; }
    if (orbitRadius > 1500.0f) { orbitRadius = 1500.0f; }
}

void Camera::orbitPose(irr::core::vector3df& pos, irr::core::vector3df& fwd) const
{
    const irr::f32 yaw = (orbitHeading + orbitYaw) * irr::core::DEGTORAD;
    const irr::f32 pitch = orbitPitch * irr::core::DEGTORAD;
    const irr::core::vector3df dir(std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch));
    pos = orbitCentre + dir * orbitRadius;
    //Kept above the sea (free view in a storm): raised, still looking at the centre
    if (pos.Y < orbitMinHeight) { pos.Y = orbitMinHeight; }
    fwd = (orbitCentre - pos);
    fwd.normalize();
}
//KYARA FEUX EDIT ^^^ ------------------------------------------------------------------------------

void Camera::update(irr::f32 deltaTime, irr::core::quaternion quat, irr::core::vector3df pos, irr::core::vector2df lensShift, bool vrMode)
{
     //link camera rotation to shipNode
        //Adjust camera angle if panning
        lookAngle += horizontalPanSpeed * deltaTime;
        while (lookAngle>=360) {
            lookAngle -= 360;
        }
        while (lookAngle<0) {
            lookAngle += 360;
        }

        lookUpAngle += verticalPanSpeed * deltaTime;
        if (lookUpAngle > maxLookUpAngle) {
            lookUpAngle = maxLookUpAngle;
        }
        if (lookUpAngle < minLookUpAngle) {
            lookUpAngle = minLookUpAngle;
        }

        // get position and transformation matrix of parent node, unless camera is static
        if (!frozen) {
            parentAngles.setRotationDegrees(parent->getRotation());
            parentPosition = parent->getPosition();
        }

        //KYARA FEUX EDIT: orbiting a lamp replaces the bridge view entirely
        if (orbiting) {
            irr::core::vector3df orbitPos, orbitFwd;
            orbitPose(orbitPos, orbitFwd);
            frv = orbitFwd;
            camera->setLensShift(lensShift);
            camera->setPosition(orbitPos);
            camera->setUpVector(irr::core::vector3df(0.0f, 1.0f, 0.0f));
            camera->setTarget(orbitPos + orbitFwd);
            camera->updateAbsolutePosition();
            return;
        }

        // Quaternion for the view angles, ignoring the lookUpAngle if in VR Mode
        irr::core::quaternion viewQuat;
        if (vrMode) {
            viewQuat = irr::core::quaternion(0,
                                             irr::core::DEGTORAD*(lookAngle-angleCorrection),
                                             0);
        } else {
            viewQuat = irr::core::quaternion(irr::core::DEGTORAD*(-1*lookUpAngle),
                                             irr::core::DEGTORAD*(lookAngle-angleCorrection),
                                             0);
        }

        // transform forward vector of camera
        frv = irr::core::vector3df(0.0f,0.0f,1.0f);
        frv=quat*frv;
        frv=viewQuat*frv;
        parentAngles.transformVect(frv);

        // transform upvector of camera
        irr::core::vector3df upv(0.0f, 1.0f, 0.0f);
        upv = quat*upv;
        upv = viewQuat*upv;
        parentAngles.transformVect(upv);

        // Update side view vector (must be perpendicular to upv and frv)
        sideViewVector = upv.crossProduct(frv);

        // transform camera offset ('offset' is relative to the local ship coordinates, and stays the same.)
        //'offsetTransformed' is transformed into the global coordinates
        irr::core::vector3df offsetTransformed;
        parentAngles.transformVect(offsetTransformed,views[currentView] + pos);

        // Set lens shift (TODO: avoid recalculation)
        camera->setLensShift(lensShift);

        //move camera and angle
        irr::core::vector3df cameraPosition = parentPosition + offsetTransformed;
        camera->setPosition(cameraPosition);
        camera->setUpVector(upv); //set up vector of camera
        camera->setTarget(cameraPosition + frv); //set target of camera (look at point)
        camera->updateAbsolutePosition();

        //also set rotation, so we can get camera's direction
        camera->setRotation(parent->getRotation());

}

//NAUTITECH triple-screen: render a single angled column (one physical TV) of the
//Eyefinity surface. Reuses parentPosition / parentAngles / lookAngle / lookUpAngle
//already set by the most recent update() call, so no extra model tick is needed.
//Non-VR path (identity head quaternion, no positional offset, no lens shift).
void Camera::renderColumn(irr::f32 columnAspect, irr::f32 columnHFOVrad, irr::f32 yawOffsetDeg, irr::f32 shiftY)
{
    // Per-column aspect + field of view
    camera->setAspectRatio(columnAspect);
    hFOV = columnHFOVrad;
    irr::f32 vFOV = 2*atan(tan(hFOV/2)/columnAspect);
    camera->setFOV(vFOV);
    camera->setLensShift(irr::core::vector2df(0, shiftY));

    //KYARA FEUX EDIT: triple-screen columns turn about the orbit position too
    if (orbiting) {
        irr::core::vector3df orbitPos, orbitFwd;
        orbitPose(orbitPos, orbitFwd);
        irr::core::quaternion yawQ(0.0f, irr::core::DEGTORAD * yawOffsetDeg, 0.0f);
        orbitFwd = yawQ * orbitFwd;
        camera->setPosition(orbitPos);
        camera->setUpVector(irr::core::vector3df(0.0f, 1.0f, 0.0f));
        camera->setTarget(orbitPos + orbitFwd);
        camera->updateAbsolutePosition();
        return;
    }

    // Rebuild the view direction with an extra yaw for this panel
    irr::core::quaternion viewQuat(irr::core::DEGTORAD*(-1*lookUpAngle),
                                   irr::core::DEGTORAD*(lookAngle-angleCorrection + yawOffsetDeg),
                                   0);

    irr::core::vector3df frvLocal(0.0f,0.0f,1.0f);
    frvLocal = viewQuat*frvLocal;
    parentAngles.transformVect(frvLocal);

    irr::core::vector3df upv(0.0f,1.0f,0.0f);
    upv = viewQuat*upv;
    parentAngles.transformVect(upv);

    irr::core::vector3df offsetTransformed;
    parentAngles.transformVect(offsetTransformed, views[currentView]);
    irr::core::vector3df cameraPosition = parentPosition + offsetTransformed;

    camera->setPosition(cameraPosition);
    camera->setUpVector(upv);
    camera->setTarget(cameraPosition + frvLocal);
    camera->updateAbsolutePosition();
}
//NAUTITECH triple-screen picking: build a world-space pick ray for a given column
//directly from the camera vectors, independent of the cached view frustum (which only
//refreshes during a render). ndcX/ndcY are -1..1 with centre 0.
irr::core::line3df Camera::getPickRay(irr::f32 ndcX, irr::f32 ndcY, irr::f32 columnHFOVrad, irr::f32 columnAspect, irr::f32 yawOffsetDeg)
{
    irr::core::quaternion viewQuat(irr::core::DEGTORAD * (-1 * lookUpAngle),
        irr::core::DEGTORAD * (lookAngle - angleCorrection + yawOffsetDeg),
        0);

    irr::core::vector3df forward(0.0f, 0.0f, 1.0f);
    forward = viewQuat * forward;
    parentAngles.transformVect(forward);
    forward.normalize();

    irr::core::vector3df up(0.0f, 1.0f, 0.0f);
    up = viewQuat * up;
    parentAngles.transformVect(up);
    up.normalize();

    irr::core::vector3df right = up.crossProduct(forward); // +X / starboard, matches sideViewVector
    right.normalize();

    irr::f32 tanHalfH = tan(columnHFOVrad * 0.5f);
    irr::f32 tanHalfV = tanHalfH / columnAspect;

    irr::core::vector3df dir = forward
        + right * (tanHalfH * ndcX)
        + up * (tanHalfV * ndcY);
    dir.normalize();

    irr::core::vector3df origin = camera->getAbsolutePosition();
    irr::f32 rayLength = 20000.0f;
    return irr::core::line3df(origin, origin + dir * rayLength);
}

void Camera::scaleViews(irr::f32 factor)
{
    if (factor <= 0.0f) { return; }
    for (size_t i = 0; i < views.size(); i++) { views[i] *= factor; }
}
