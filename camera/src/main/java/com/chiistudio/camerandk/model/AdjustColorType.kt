package com.chiistudio.camerandk.model

enum class AdjustColorType(val shaderGl: String) {
    MIX_RED("hsl = mixColorRed(hsl, u_red_shift.x, u_red_shift.y, u_red_shift.z);"),
    MIX_ORANGE("hsl = mixColorOrange(hsl, u_orange_shift.x, u_orange_shift.y, u_orange_shift.z);"),
    MIX_YELLOW("hsl = mixColorYellow(hsl, u_yellow_shift.x, u_yellow_shift.y, u_yellow_shift.z);"),
    MIX_GREEN("hsl = mixColorGreen(hsl, u_green_shift.x, u_green_shift.y, u_green_shift.z);"),
    MIX_CYAN("hsl = mixColorCyan(hsl, u_cyan_shift.x, u_cyan_shift.y, u_cyan_shift.z);"),
    MIX_BLUE("hsl = mixColorBlue(hsl, u_blue_shift.x, u_blue_shift.y, u_blue_shift.z);"),
    MIX_PURPLE("hsl = mixColorPurple(hsl, u_purple_shift.x, u_purple_shift.y, u_purple_shift.z);"),
    MIX_PINK("hsl = mixColorPink(hsl, u_pink_shift.x, u_pink_shift.y, u_pink_shift.z);"),
}