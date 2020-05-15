package org.megaf.app_client

enum class MoistureLevel {
    LOW,
    MED,
    HI;

    override fun toString() = when (this) {
        LOW -> "Low"
        MED -> "Medium"
        HI -> "High"
        else -> "???"
    }
}
