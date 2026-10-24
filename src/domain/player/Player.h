#pragma once

#include <string>
#include <utility>

#include "domain/player/ArcStatus.h"
#include "domain/player/MembershipType.h"

namespace pokersoc::domain {

/// A club member or guest, keyed by the `id` scanned from their card (barcode).
///
/// Unlike the immutable value objects (Money, Denomination), a Player is a
/// mutable entity: staff update the profile over time, so the attributes have
/// setters. Identity lives in `id` alone — two Players are "the same" when their
/// ids match, regardless of the rest of the profile.
class Player {
public:
    Player() = default;
    Player(std::string id, std::string name)
        : id_(std::move(id)), name_(std::move(name)) {}

    // ---- identity ----
    const std::string& id() const noexcept { return id_; }
    bool hasId() const noexcept { return !id_.empty(); }

    // ---- profile (queries) ----
    const std::string& name() const noexcept { return name_; }
    const std::string& email() const noexcept { return email_; }
    const std::string& studentNumber() const noexcept { return studentNumber_; }
    const std::string& degree() const noexcept { return degree_; }
    /// Free text such as "First year". "None" means it was not given.
    const std::string& studyYear() const noexcept { return studyYear_; }
    ArcStatus arcStatus() const noexcept { return arcStatus_; }
    MembershipType membership() const noexcept { return membership_; }
    bool isUnderage() const noexcept { return isUnderage_; }
    const std::string& createdAt() const noexcept { return createdAt_; }

    // ---- profile (mutations) ----
    void setName(std::string name) { name_ = std::move(name); }
    void setEmail(std::string email) { email_ = std::move(email); }
    void setStudentNumber(std::string studentNumber) { studentNumber_ = std::move(studentNumber); }
    void setDegree(std::string degree) { degree_ = std::move(degree); }
    void setStudyYear(std::string studyYear);
    void setArcStatus(ArcStatus status) noexcept { arcStatus_ = status; }
    void setMembership(MembershipType membership) noexcept { membership_ = membership; }
    void setUnderage(bool underage) noexcept { isUnderage_ = underage; }
    /// The store owns the creation timestamp; this is set when replaying a row.
    void setCreatedAt(std::string createdAt) { createdAt_ = std::move(createdAt); }

    /// One-line human summary for logs / receipts.
    std::string describe() const;

private:
    std::string id_;
    std::string name_;
    std::string email_;
    std::string studentNumber_;
    std::string degree_;
    std::string studyYear_ = "None";
    ArcStatus arcStatus_ = ArcStatus::Unknown;
    MembershipType membership_ = MembershipType::None;
    bool isUnderage_ = false;
    std::string createdAt_; // ISO-8601, assigned by the store on first save
};

} // namespace pokersoc::domain
