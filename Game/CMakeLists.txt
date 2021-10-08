cmake_minimum_required(VERSION 3.0)


# プロジェクト名
project("Game")




#==================================================
# 対象ファイルのリストを作成
#==================================================

# ALL_BUILD, ZERO_CHECK をフォルダ CMakePredefinedTargets にまとめる
SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)

file(GLOB m_files "./*.*")




#==================================================
# ソースファイルのグループ分け
#==================================================
#SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)
#source_group("" FILES ${m_files})




#==================================================
# STATIC ライブラリを作成するように設定
#==================================================
add_library(${PROJECT_NAME} STATIC ${m_files})




# 親ディレクトリ内の CMakeLists.txt で既に定義済みであるという前提で、
# ライブラリプロジェクト用の共通設定マクロ "global_lib_setup" を呼ぶ。
global_lib_setup()
