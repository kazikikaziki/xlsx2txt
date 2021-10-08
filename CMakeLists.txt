# coding: utf8
cmake_minimum_required(VERSION 3.0)

# プロジェクト名
set(m_project_name "xlsx2txt")

# Kamilo が置いてある場所
set(m_path_kamilo "Kamilo")

# ゲーム本体が置いてある場所
set(m_path_game "Game")


#===================================================================================
# マクロをインクルード
#===================================================================================
include("${m_path_kamilo}/CMake_heliodor.txt")




#===================================================================================
# 実行ファイルプロジェクト用の設定。
# このマクロは各サブディレクトリにある実行ファイル用の CMakeLists.txt から呼ばれる
#===================================================================================
macro(global_exe_setup output_name)
	heliodor_easy_exe_setup(${output_name})
	# include パスを通す
	# ※このマクロは他の CMakeLists から呼ばれるので、パスを "./lib" などとしてしまうと
	# 呼び出し側の CMakeLists が置いてあるディレクトリからの相対指定になってしまう
	include_directories("${CMAKE_SOURCE_DIR}/${m_path_kamilo}") 
	target_link_libraries(${PROJECT_NAME} "Kamilo")

	include_directories("${CMAKE_SOURCE_DIR}/${m_path_game}") 
	target_link_libraries(${PROJECT_NAME} "Game")
endmacro()


# ライブラリプロジェクト用の共通設定。
# このマクロは各サブディレクトリにあるライブラリ用の CMakeLists.txt から呼ばれる
macro(global_lib_setup)
	heliodor_easy_lib_setup()
	# ライブラリのパスを通す
	# ※このマクロは他の CMakeLists から呼ばれるので、パスを "./lib" などとしてしまうと
	# 呼び出し側の CMakeLists が置いてあるディレクトリからの相対指定になってしまう
	include_directories("${CMAKE_SOURCE_DIR}/${m_path_kamilo}") 
	include_directories("${CMAKE_SOURCE_DIR}/${m_path_game}") 
endmacro()




#===================================================================================
# プロジェクト
#===================================================================================
project(${m_project_name})


set(KENG_DIRECTX_SDK2004 1) # DirectX SDK2004 を使うように選択しておく

add_subdirectory(./${m_path_kamilo})
add_subdirectory(./${m_path_game})
file(GLOB m_files
	"./*.cpp"
	"./*.h"
)
source_group("" FILES ${m_files})
add_executable(${PROJECT_NAME}
	${m_files}
)
global_exe_setup(${PROJECT_NAME})
