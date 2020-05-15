package org.megaf.app_client

import kotlinx.serialization.Serializable

@Serializable
class StatePoko(
    val temp: Double,
    val humidity: Double,
    val waterLevel: Int,
    val moistureLevel: Int
)



