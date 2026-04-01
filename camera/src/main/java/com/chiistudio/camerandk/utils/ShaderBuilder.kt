package com.chiistudio.camerandk.utils

import android.content.Context
import com.chiistudio.camerandk.model.AdjustType

object ShaderBuilder {
    fun buildShader(
        context: Context,
        pathMain: String,
        effect: String,
        filter: String
    ): String {
        val builder = StringBuilder()
        builder.append(AssetReader.readString(context, pathMain))
        builder.append(AssetReader.readString(context, "glsl/frag_base_shader_blend_v2.glsl"))
        builder.append(AssetReader.readString(context, "glsl/frag_base_shader_adjust_v2.glsl"))
        builder.append(filter)
        builder.append(effect)
        val main = """
            void main(){
                vec2 uv = scaleTexture(v_texCoord, u_scale);
                vec4 color;
                if (uv.x < 0.0 || uv.x > 1.0 || uv.y<0.0 ||uv.y>1.0){
                    color = vec4(0.0, 0.0, 0.0, 1.0);
                } else {
                    color = addEffect(uv);
                    color = addFilter(color);
                    %s
                }
                fragColor = color;
            }
        """.trimIndent().format(AdjustType.entries.joinToString (""){ it.glShader })
        builder.append(main)
        return builder.toString()
    }
}