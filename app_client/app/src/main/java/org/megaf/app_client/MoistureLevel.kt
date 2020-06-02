package org.megaf.app_client

enum class MoistureLevel {
    LOW,
    MED,
    MED_P,
    HI;


    override fun toString() = when (this) {
        LOW -> "Low"
        MED -> "Medium"
        HI -> "High"
        else -> "???"
    }
}
