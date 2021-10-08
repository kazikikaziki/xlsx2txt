cmake_minimum_required(VERSION 3.0)


# プロジェクト名
project(Kamilo)


# よくつかうマクロをインクルード
include("CMake_heliodor.txt")




#==================================================
# 対象ファイルのリストを作成
#==================================================
file(GLOB m_files_src
	"./*.c"
	"./*.cpp"
	"./*.h"
)
file(GLOB m_files_imgui
	"./imgui/*.cpp"
	"./imgui/*.h"
)
file(GLOB m_files_lua
	"./lua/*.c"
	"./lua/*.h"
)
file(GLOB m_files_miniz
	"./miniz/*.c"
	"./miniz/*.h"
)
file(GLOB m_files_tinyxml
	"./tinyxml/*.cpp"
	"./tinyxml/*.h"
)




#==================================================
# ソースファイルのグループ分け
#==================================================
source_group(""        FILES ${m_files_src})
source_group("imgui"   FILES ${m_files_imgui})
source_group("lua"     FILES ${m_files_lua})
source_group("miniz"   FILES ${m_files_miniz})
source_group("tinyxml" FILES ${m_files_tinyxml})




#==================================================
# STATIC ライブラリを作成するように設定
#==================================================
add_library(${PROJECT_NAME} STATIC
	${m_files_src}
	${m_files_imgui}
	${m_files_lua}
	${m_files_miniz}
	${m_files_tinyxml}
)




#==================================================
# CMakeのオプション項目に表示する値
#==================================================
#heliodor_set_option(KENG_NO_ASSERT         FALSE BOOL "Define 'KENG_NO_ASSERT' to remove all assertion calls") # assert の無効化
#heliodor_set_option(KENG_NO_STRICT_CHECK   FALSE BOOL "Define 'KENG_NO_STRICT_CHECK' to avoid strict checking code") # 検証コードの無効化




#==================================================
# マクロを定義
#==================================================
#if (KENG_NO_ASSERT)
#	target_compile_definitions(${PROJECT_NAME} PRIVATE KENG_NO_ASSERT)
#endif()
#if (KENG_NO_STRICT_CHECK)
#	target_compile_definitions(${PROJECT_NAME} PRIVATE KENG_NO_STRICT_CHECK)
#endif()




#==================================================
# ビルド設定
#==================================================
heliodor_configurations() # Debug Release
heliodor_definitions()    # _CRT_SECURE_NO_WARNINGS, NOMINMAX
heliodor_static_runtime() # MT, MTd
heliodor_link_win32()     # Win32ライブラリをリンク
heliodor_link_d3d9()      # DirectX9ライブラリをリンク
