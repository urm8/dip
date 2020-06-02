package org.megaf.app_client

import kotlinx.serialization.Serializable

class StatePoko(
    val temp: Double,
    val moistureLevel: Int,
    val waterLevel: Int = 0,
    val secondsSinceLastMoisture: Long = 0
)



