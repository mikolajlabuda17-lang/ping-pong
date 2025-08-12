package com.example.game

import android.graphics.Color

data class Fighter(
    val name: String,
    val color: Int = Color.BLUE,
    val maxHp: Int = 30,
    var hp: Int = maxHp
) {
    fun isAlive(): Boolean = hp > 0
    fun reset() { hp = maxHp }
    fun takeDamage(amount: Int) { hp = (hp - amount).coerceAtLeast(0) }
}