// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2014 projectchrono.org
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Alessandro Tasora, Radu Serban
// =============================================================================

#include "chrono/solver/ChConstraintThreeGeneric.h"

namespace chrono {

// Register into the object factory, to enable run-time dynamic creation and persistence
CH_FACTORY_REGISTER(ChConstraintThreeGeneric)

ChConstraintThreeGeneric::ChConstraintThreeGeneric() {}

ChConstraintThreeGeneric::ChConstraintThreeGeneric(ChVariables* mvariables_a,
                                                   ChVariables* mvariables_b,
                                                   ChVariables* mvariables_c) {
    SetVariables(mvariables_a, mvariables_b, mvariables_c);
}

ChConstraintThreeGeneric::ChConstraintThreeGeneric(const ChConstraintThreeGeneric& other) : ChConstraintThree(other) {
    Cq_a = other.Cq_a;
    Cq_b = other.Cq_b;
    Cq_c = other.Cq_c;
    Eq_a = other.Eq_a;
    Eq_b = other.Eq_b;
    Eq_c = other.Eq_c;
}

ChConstraintThreeGeneric& ChConstraintThreeGeneric::operator=(const ChConstraintThreeGeneric& other) {
    if (&other == this)
        return *this;

    // copy parent class data
    ChConstraintThree::operator=(other);

    Cq_a = other.Cq_a;
    Cq_b = other.Cq_b;
    Cq_c = other.Cq_c;
    Eq_a = other.Eq_a;
    Eq_b = other.Eq_b;
    Eq_c = other.Eq_c;

    return *this;
}

void ChConstraintThreeGeneric::SetVariables(ChVariables* mvariables_a,
                                            ChVariables* mvariables_b,
                                            ChVariables* mvariables_c) {
    if (!mvariables_a || !mvariables_b || !mvariables_c) {
        SetValid(false);
        return;
    }

    SetValid(true);
    variables_a = mvariables_a;
    variables_b = mvariables_b;
    variables_c = mvariables_c;

    if (variables_a->Get_ndof() > 0) {
        Cq_a.resize(variables_a->Get_ndof());
        Eq_a.resize(variables_a->Get_ndof());
        Cq_a.setZero();
    }

    if (variables_b->Get_ndof() > 0) {
        Cq_b.resize(variables_b->Get_ndof());
        Eq_b.resize(variables_b->Get_ndof());
        Cq_b.setZero();
    }

    if (variables_c->Get_ndof() > 0) {
        Cq_c.resize(variables_c->Get_ndof());
        Eq_c.resize(variables_c->Get_ndof());
        Cq_c.setZero();
    }
}

void ChConstraintThreeGeneric::Update_auxiliary() {
    // 1- Assuming jacobians are already computed, now compute
    //   the matrices [Eq_a]=[invM_a]*[Cq_a]' and [Eq_b]
    if (variables_a->IsActive() && variables_a->Get_ndof() > 0) {
        variables_a->Compute_invMb_v(Eq_a, Cq_a);
    }
    if (variables_b->IsActive() && variables_b->Get_ndof() > 0) {
        variables_b->Compute_invMb_v(Eq_b, Cq_b);
    }
    if (variables_c->IsActive() && variables_c->Get_ndof() > 0) {
        variables_c->Compute_invMb_v(Eq_c, Cq_b);
    }

    //// RADU
    //// How can I include the conditions in a single Eigen expression?
    //// Option:
    ////         int a = (variables_a->IsActive() && variables_a->Get_ndof() > 0) ? 1 : 0;
    ////         int b = (variables_b->IsActive() && variables_b->Get_ndof() > 0) ? 1 : 0;
    ////         int c = (variables_c->IsActive() && variables_c->Get_ndof() > 0) ? 1 : 0;
    ////         g_i = a * Cq_a.dot(Eq_a) + b * Cq_b.dot(Eq_b) + c * Cq_c.dot(Eq_c);
    //// Is it worth it?

    // 2- Compute g_i = [Cq_i]*[invM_i]*[Cq_i]' + cfm_i
    ChMatrixDynamic<double> res(1, 1);
    g_i = 0;
    if (variables_a->IsActive() && variables_a->Get_ndof() > 0) {
        g_i += Cq_a.dot(Eq_a);
    }
    if (variables_b->IsActive() && variables_b->Get_ndof() > 0) {
        g_i += Cq_b.dot(Eq_b);
    }
    if (variables_c->IsActive() && variables_c->Get_ndof() > 0) {
        g_i += Cq_c.dot(Eq_c);
    }

    // 3- adds the constraint force mixing term (usually zero):
    if (cfm_i != 0)
        g_i += cfm_i;
}

double ChConstraintThreeGeneric::Compute_Cq_q() {
    double ret = 0;

    if (variables_a->IsActive()) {
        ret += Cq_a.dot(variables_a->Get_qb());
    }

    if (variables_b->IsActive()) {
        ret += Cq_b.dot(variables_b->Get_qb());
    }

    if (variables_c->IsActive()) {
        ret += Cq_c.dot(variables_c->Get_qb());
    }

    return ret;
}

void ChConstraintThreeGeneric::Increment_q(const double deltal) {
    if (variables_a->IsActive()) {
        variables_a->Get_qb() += Eq_a * deltal;
    }

    if (variables_b->IsActive()) {
        variables_b->Get_qb() += Eq_b * deltal;
    }

    if (variables_c->IsActive()) {
        variables_c->Get_qb() += Eq_c * deltal;
    }
}

//// RADU
//// ATTENTION: previously there were bugs in the following two functions!
////     Indeed, the for loops wer using Cq_a->GetRows().   But that is always 1...

void ChConstraintThreeGeneric::MultiplyAndAdd(double& result, const ChVectorDynamic<double>& vect) const {
    if (variables_a->IsActive()) {
        result += Cq_a.dot(vect.segment(variables_a->GetOffset(), Cq_a.size()));
    }

    if (variables_b->IsActive()) {
        result += Cq_b.dot(vect.segment(variables_b->GetOffset(), Cq_b.size()));
    }

    if (variables_c->IsActive()) {
        result += Cq_c.dot(vect.segment(variables_c->GetOffset(), Cq_c.size()));
    }
}

void ChConstraintThreeGeneric::MultiplyTandAdd(ChVectorDynamic<double>& result, double l) {
    if (variables_a->IsActive()) {
        result.segment(variables_a->GetOffset(), Cq_a.size()) += Cq_a * l;
    }

    if (variables_b->IsActive()) {
        result.segment(variables_b->GetOffset(), Cq_b.size()) += Cq_b * l;
    }

    if (variables_c->IsActive()) {
        result.segment(variables_c->GetOffset(), Cq_c.size()) += Cq_c * l;
    }
}

void ChConstraintThreeGeneric::Build_Cq(ChSparseMatrix& storage, int insrow) {
    // Recall that Cq_a, Cq_b, and Cq_c are column vectors.
    if (variables_a->IsActive())
        storage.PasteTranspMatrix(Cq_a, insrow, variables_a->GetOffset());
    if (variables_b->IsActive())
        storage.PasteTranspMatrix(Cq_b, insrow, variables_b->GetOffset());
    if (variables_c->IsActive())
        storage.PasteTranspMatrix(Cq_c, insrow, variables_c->GetOffset());
}

void ChConstraintThreeGeneric::Build_CqT(ChSparseMatrix& storage, int inscol) {
    // Recall that Cq_a, Cq_b, and Cq_c are column vectors.
    if (variables_a->IsActive())
        storage.PasteMatrix(Cq_a, variables_a->GetOffset(), inscol);
    if (variables_b->IsActive())
        storage.PasteMatrix(Cq_b, variables_b->GetOffset(), inscol);
    if (variables_c->IsActive())
        storage.PasteMatrix(Cq_c, variables_c->GetOffset(), inscol);
}

void ChConstraintThreeGeneric::ArchiveOUT(ChArchiveOut& marchive) {
    // version number
    marchive.VersionWrite<ChConstraintThreeGeneric>();

    // serialize the parent class data too
    ChConstraintThree::ArchiveOUT(marchive);

    // serialize all member data:
    // NOTHING INTERESTING TO SERIALIZE (the Cq jacobians are not so
    // important to waste disk space.. they may be recomputed run-time,
    // and pointers to variables must be rebound in run-time.)
    // mstream << Cq_a;
    // mstream << Cq_b;
}

void ChConstraintThreeGeneric::ArchiveIN(ChArchiveIn& marchive) {
    // version number
    int version = marchive.VersionRead<ChConstraintThreeGeneric>();

    // deserialize the parent class data too
    ChConstraintThree::ArchiveIN(marchive);

    // deserialize all member data:
    // NOTHING INTERESTING TO SERIALIZE (the Cq jacobians are not so
    // important to waste disk space.. they may be recomputed run-time,
    // and pointers to variables must be rebound in run-time.)
    // mstream << Cq_a;
    // mstream << Cq_b;
}

}  // end namespace chrono
