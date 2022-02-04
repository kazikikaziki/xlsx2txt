# coding: utf-8

#
# カレントフォルダ内にある【CMakeLists.txt】を CMake に渡して .sln ファイルを作り、ついでにビルドする
#
# ※あらかじめ CMake をインストールしておくこと
# CMake インストール時に「環境変数に CMake を追加する」にチェックをつけて、パスを通しておくこと
#
# https://cmake.org/cmake/help/v3.1/manual/cmake.1.html
# https://stackoverflow.com/questions/31148943/option-to-force-either-32-bit-or-64-bit-build-with-cmake
#

import os
import subprocess
import shutil
import traceback

# cmake 用の作業フォルダ名
g_OutputDir = "__cmake"


# 最初のカレントフォルダ
g_HomeDir = os.getcwd()





#----------------------------------------------------
# コマンドラインを実行する
def execute(cmd):
	print(">>>> ")
	print(">>>> " + cmd)
	print(">>>> ")
	try:
		subprocess.check_call(cmd, shell=True)
		return True
	except:
		return False


#----------------------------------------------------
# cmake 用の作業フォルダを作成する
def make_cmake_output_dir():
	if not os.path.isdir(g_OutputDir):
		os.mkdir(g_OutputDir)


#----------------------------------------------------
# cmake 用の作業フォルダを削除する
def remove_cmake_output_dir():
	if os.path.isdir(g_OutputDir):
		# 安全のため、絶対パスでの指定は拒否する
		if os.path.isabs(g_OutputDir):
			raise RuntimeError
		
		# 安全のため、上方向へのパスを含む相対パスを拒否する
		if ".." in g_OutputDir: # ..を含むフォルダ名も弾くことになるが、良い名前とは言えないので、それで良しとする
			raise RuntimeError
		
		shutil.rmtree(g_OutputDir)


#----------------------------------------------------
# cmake ファイルを作成。
def execute_cmake(args):
	# 既存の cmake 出力フォルダを削除
	remove_cmake_output_dir();

	# cmake 出力フォルダを作成
	make_cmake_output_dir();
	
	# cmake を実行
	# cmake はカレントディレクトリに対して生成物を出力するため、
	# まず最初に出力用フォルダを作っておき、そこにカレントディレクトリを移動してから実行する。
	# 第1引数には入力用の CMakeLiists.txt が存在するフォルダを指定する
	os.chdir(g_OutputDir)
	result = execute("cmake .. " + args)
	os.chdir(g_HomeDir) # 戻す

	# 失敗したら cmake 出力フォルダを消す
	if not result:
		remove_cmake_output_dir();

	return result


#----------------------------------------------------
def execute_cmake_try():
	if execute_cmake('-G"Visual Studio 17 2022" -AWin32'): # 32ビット版を作成する
		return True
	if execute_cmake('-G"Visual Studio 16 2019" -AWin32'): # 32ビット版を作成する
		return True
	return False


#----------------------------------------------------
try:
	if execute_cmake_try():
		execute("cmake --build .") # ついでに exe のビルドテストもする
		
except:
	traceback.print_exc()


# 終了
input(u"[エンターキーで終了]")

