// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2014 projectchrono.org
// All right reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Radu Serban
// =============================================================================
//
// Base class for a track assembly which consists of one sprocket, one idler,
// a collection of road wheel assemblies (suspensions), and a collection of
// track shoes.
//
// The reference frame for a vehicle follows the ISO standard: Z-axis up, X-axis
// pointing forward, and Y-axis towards the left of the vehicle.
//
// =============================================================================

#include <cmath>

#include "chrono/core/ChLog.h"

#include "chrono_vehicle/tracked_vehicle/ChTrackAssembly.h"

namespace chrono {
namespace vehicle {

// -----------------------------------------------------------------------------
// Get the complete state for the specified track shoe.
// -----------------------------------------------------------------------------
BodyState ChTrackAssembly::GetTrackShoeState(size_t id) const {
    BodyState state;

    state.pos = GetTrackShoePos(id);
    state.rot = GetTrackShoeRot(id);
    state.lin_vel = GetTrackShoeLinVel(id);
    state.ang_vel = GetTrackShoeAngVel(id);

    return state;
}

// -----------------------------------------------------------------------------
// Initialize this track assembly subsystem.
// -----------------------------------------------------------------------------
void ChTrackAssembly::Initialize(ChSharedPtr<ChBodyAuxRef> chassis,
                                 const ChVector<>& sprocket_loc,
                                 const ChVector<>& idler_loc,
                                 const std::vector<ChVector<> >& suspension_locs) {
    m_sprocket->Initialize(chassis, sprocket_loc, this);
    m_idler->Initialize(chassis, idler_loc);
    m_brake->Initialize(m_sprocket->GetRevolute());

    for (size_t i = 0; i < m_suspensions.size(); ++i) {
        m_suspensions[i]->Initialize(chassis, suspension_locs[i]);
    }

    // Assemble the track. This positions all track shoes around the sprocket,
    // road wheels, and idler.
    bool ccw = Assemble(chassis);

    // Loop over all track shoes and allow them to connect themselves to their
    // neighbor.
    size_t num_shoes = m_shoes.size();
    ChSharedPtr<ChTrackShoe> next;
    for (size_t i = 0; i < num_shoes; ++i) {
        if (ccw)
            next = (i == num_shoes - 1) ? m_shoes[0] : m_shoes[i + 1];
        else
            next = (i == 0) ? m_shoes[num_shoes - 1] : m_shoes[i - 1];
        m_shoes[i]->Connect(next);
    }
}

// -----------------------------------------------------------------------------
// Assemble track shoes over wheels.
//
// Returns true if the track shoes were initialized in a counter clockwise
// direction and false otherwise.
//
// This procedure is performed in the chassis reference frame, taking into
// account the convention that the chassis reference frame has the x-axis
// pointing to the front of the vehicle and the z-axis pointing up.
// It is also assumed that the sprocket, idler, and road wheels lie in the
// same vertical plane (in the chassis reference frame). The assembly is done
// in the (z-x) plane.
//
// TODO: may need fixes for clock-wise wrapping (idler in front of sprocket)
//
// -----------------------------------------------------------------------------
bool ChTrackAssembly::Assemble(ChSharedPtr<ChBodyAuxRef> chassis) {
    // Number of track shoes and road wheels.
    size_t num_shoes = m_shoes.size();
    size_t num_wheels = m_suspensions.size();
    size_t index = 0;

    // Positions of sprocket, idler, and (front and rear) wheels.
    const ChVector<>& sprocket_pos = chassis->TransformPointParentToLocal(m_sprocket->GetGearBody()->GetPos());
    const ChVector<>& idler_pos = chassis->TransformPointParentToLocal(m_idler->GetWheelBody()->GetPos());

    ChVector<> front_wheel_pos = chassis->TransformPointParentToLocal(m_suspensions[0]->GetWheelBody()->GetPos());
    ChVector<> rear_wheel_pos = front_wheel_pos;
    for (size_t i = 1; i < num_wheels; i++) {
        const ChVector<>& wheel_pos = chassis->TransformPointParentToLocal(m_suspensions[i]->GetWheelBody()->GetPos());
        if (wheel_pos.x > front_wheel_pos.x)
            front_wheel_pos = wheel_pos;
        if (wheel_pos.x < rear_wheel_pos.x)
            rear_wheel_pos = wheel_pos;
    }

    // Subsystem parameters.
    // Note that the idler and wheel radii are inflated by a fraction of the shoe height.
    double shoe_pitch = m_shoes[0]->GetPitch();
    double shoe_height = m_shoes[0]->GetHeight();
    double sprocket_radius = m_sprocket->GetAssemblyRadius();
    double idler_radius = m_idler->GetWheelRadius() + 1.0 * shoe_height;
    double wheel_radius = m_suspensions[0]->GetWheelRadius() + 0.9 * shoe_height;

    // Decide whether we wrap counter-clockwise (sprocket in front of idler) or
    // clockwise (sprocket behind idler).
    bool ccw = sprocket_pos.x > idler_pos.x;
    double sign = ccw ? -1 : +1;
    const ChVector<>& wheel_sprocket_pos = ccw ? front_wheel_pos : rear_wheel_pos;
    const ChVector<>& wheel_idler_pos = ccw ? rear_wheel_pos : front_wheel_pos;

    // 1. Create shoes around the sprocket, starting under the sprocket and
    //    moving away from the idler. Stop before creating a horizontal track
    //    shoe above the sprocket.

    // Initialize the location of the first shoe connection point.
    ChVector<> p0 = sprocket_pos - ChVector<>(0, 0, sprocket_radius);
    ChVector<> p1 = p0;
    ChVector<> p2;

    // Calculate the incremental pitch angle around the sprocket.
    double tmp = shoe_pitch / (2 * sprocket_radius);
    double delta_angle = sign * std::asin(tmp);
    double angle = delta_angle;

    // Create track shoes around the sprocket.
    while (std::abs(angle) < CH_C_PI && index < num_shoes) {
        p2 = p1 + shoe_pitch * ChVector<>(-sign * std::cos(angle), 0, sign * std::sin(angle));
        m_shoes[index]->Initialize(chassis, 0.5 * (p1 + p2), Q_from_AngY(angle), index);
        p1 = p2;
        angle += 2 * delta_angle;
        ++index;
    }

    // 2. Create shoes between sprocket and idler. These shoes are parallel to a
    //    line connecting the top points of the sprocket gear and idler wheel.
    //    We target a point that lies above the idler by slightly more than the
    //    track shoe's height and stop when we reach the idler location.

    // Calculate the constant pitch angle.
    double dz = (sprocket_pos.z + sprocket_radius) - (idler_pos.z + idler_radius);
    double dx = sprocket_pos.x - idler_pos.x;
    angle = ccw ? -CH_C_PI - std::atan2(dz, dx) : CH_C_PI + std::atan2(dz, -dx);

    // Create track shoes with constant orientation
    while (sign * (idler_pos.x - p2.x + 0.5 * shoe_pitch) > 0 && index < num_shoes) {
        p2 = p1 + shoe_pitch * ChVector<>(-sign * std::cos(angle), 0, sign * std::sin(angle));
        m_shoes[index]->Initialize(chassis, 0.5 * (p1 + p2), Q_from_AngY(angle), index);
        p1 = p2;
        ++index;
    }

    // 3. Create shoes around the idler wheel. Stop when we wrap under the idler.

    // Calculate the incremental pitch angle around the idler.
    tmp = shoe_pitch / (2 * idler_radius);
    delta_angle = sign * std::asin(tmp);

    while (std::abs(angle) < CH_C_2PI && index < num_shoes) {
        p2 = p1 + shoe_pitch * ChVector<>(-sign * std::cos(angle), 0, sign * std::sin(angle));
        m_shoes[index]->Initialize(chassis, 0.5 * (p1 + p2), Q_from_AngY(angle), index);
        p1 = p2;
        angle += 2 * delta_angle;
        ++index;
    }

    // 4. Create shoes between idler and closest road wheel. The shoes are parallel
    //    to a line connecting bottom points on idler and wheel. Stop when passing
    //    the wheel position.

    dz = (idler_pos.z - idler_radius) - (wheel_idler_pos.z - wheel_radius);
    dx = idler_pos.x - wheel_idler_pos.x;
    angle = ccw ? -CH_C_2PI + std::atan2(dz, -dx) : -CH_C_PI - std::atan2(dz, dx);

    // Create track shoes with constant orientation
    while (sign * (p2.x - wheel_idler_pos.x) > 0 && index < num_shoes) {
        p2 = p1 + shoe_pitch * ChVector<>(-sign * std::cos(angle), 0, sign * std::sin(angle));
        m_shoes[index]->Initialize(chassis, 0.5 * (p1 + p2), Q_from_AngY(angle), index);
        p1 = p2;
        ++index;
    }

    // 5. Create shoes below the road wheels. These shoes are horizontal. Stop when
    //    passing the position of the wheel closest to the sprocket.

    angle = ccw ? 0 : CH_C_2PI;

    while (sign * (p2.x - wheel_sprocket_pos.x) > 0 && index < num_shoes) {
        p2 = p1 + shoe_pitch * ChVector<>(-sign * std::cos(angle), 0, sign * std::sin(angle));
        m_shoes[index]->Initialize(chassis, 0.5 * (p1 + p2), Q_from_AngY(angle), index);
        p1 = p2;
        ++index;
    }

    // 6. If we have an odd number of track shoes left, create one more horizontal shoe.

    size_t num_left = num_shoes - index;

    if (num_left % 2 == 1) {
        p2 = p1 + shoe_pitch * ChVector<>(-sign * std::cos(angle), 0, sign * std::sin(angle));
        m_shoes[index]->Initialize(chassis, 0.5 * (p1 + p2), Q_from_AngY(angle), index);
        p1 = p2;
        ++index;
        --num_left;
    }

    // 7. Check if the remaining shoes are enough to close the loop.

    double gap = (p0 - p1).Length();

    if (num_left * shoe_pitch < gap) {
        GetLog() << "\nInsufficient number of track shoes for this configuration.\n";
        GetLog() << "Missing distance: " << gap - num_left * shoe_pitch << "\n\n";
        for (size_t i = index; i < num_shoes; i++) {
            p2 = p1 + shoe_pitch * ChVector<>(-sign * std::cos(angle), 0, sign * std::sin(angle));
            m_shoes[i]->Initialize(chassis, 0.5 * (p1 + p2), Q_from_AngY(angle), i);
            p1 = p2;
        }
        return ccw;
    }

    // 8. Complete the loop using the remaining shoes (always an even number)
    //    Form an isosceles triangle connecting the last initialized shoe with
    //    the very first one under the sprocket.

    double alpha = std::atan2(p0.z - p2.z, p0.x - p2.x);
    double beta = std::acos(gap / (shoe_pitch * num_left));

    // Create half of the remaining shoes (with a pitch angle = alpha-beta).
    angle = sign * (alpha - beta);
    for (size_t i = 0; i < num_left / 2; i++) {
        p2 = p1 + shoe_pitch * ChVector<>(-sign * std::cos(angle), 0, sign * std::sin(angle));
        m_shoes[index]->Initialize(chassis, 0.5 * (p1 + p2), Q_from_AngY(angle), index);
        p1 = p2;
        ++index;
    }

    // Create the second half of the remaining shoes (pitch angle = alpha+beta).
    angle = sign * (alpha + beta);
    for (size_t i = 0; i < num_left / 2; i++) {
        p2 = p1 + shoe_pitch * ChVector<>(-sign * std::cos(angle), 0, sign * std::sin(angle));
        m_shoes[index]->Initialize(chassis, 0.5 * (p1 + p2), Q_from_AngY(angle), index);
        p1 = p2;
        ++index;
    }

    GetLog() << "Track assembly done.  Number of track shoes: " << index << "\n\n";
    return ccw;
}

// -----------------------------------------------------------------------------
// Update the state of this track assembly at the current time.
// -----------------------------------------------------------------------------
void ChTrackAssembly::Update(double time, double braking, const TrackShoeForces& shoe_forces) {
    // Apply track shoe forces
    for (size_t i = 0; i < m_shoes.size(); ++i) {
        m_shoes[i]->m_shoe->Empty_forces_accumulators();
        m_shoes[i]->m_shoe->Accumulate_force(shoe_forces[i].force, shoe_forces[i].point, false);
        m_shoes[i]->m_shoe->Accumulate_torque(shoe_forces[i].moment, false);
    }

    // Apply braking input
    m_brake->Update(braking);
}

// -----------------------------------------------------------------------------
// Log constraint violations
// -----------------------------------------------------------------------------
void ChTrackAssembly::LogConstraintViolations() {
    GetLog() << "SPROCKET constraint violations\n";
    m_sprocket->LogConstraintViolations();
    GetLog() << "IDLER constraint violations\n";
    m_idler->LogConstraintViolations();
    for (size_t i = 0; i < m_suspensions.size(); i++) {
        GetLog() << "SUSPENSION #" << i << " constraint violations\n";
        m_suspensions[i]->LogConstraintViolations();
    }
}

}  // end namespace vehicle
}  // end namespace chrono
