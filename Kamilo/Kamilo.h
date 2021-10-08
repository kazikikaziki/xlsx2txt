/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php
#pragma once
#include "keng_game.h"

#include "CApp.h"
#include "COutline.h"
#include "CController.h"

#include "KAction.h"
#include "KAnimation.h"
#include "KAny.h"
#include "KAudioPlayer.h"
#include "KAutoHideCursor.h"
#include "KCamera.h"
#include "KChunkedFile.h"
#include "KClipboard.h"
#include "KClock.h"
#include "KCollisionShape.h"
#include "KCrashReport.h"
#include "KCrc32.h"
#include "KDebug.h"
#include "KDialog.h"
#include "KDirectoryWalker.h"
#include "KDrawable.h"
#include "KEasing.h"
#include "KEmbeddedFiles.h"
#include "KExcel.h"
#include "KFont.h"
#include "KGizmoAct.h"
#include "KHitbox.h"
#include "KImage.h"
#include "KImagePack.h"
#include "KImGui.h"
#include "KInputMap.h"
#include "KInspector.h"
#include "KInternal.h"
#include "KJobQueue.h"
#include "KLocalTIme.h"
#include "KLog.h"
#include "KLua.h"
#include "KMath.h"
#include "KMatrix.h"
#include "KMeshDrawable.h"
#include "KNamedValues.h"
#include "KNode.h"
#include "KPac.h"
#include "KQuat.h"
#include "KRand.h"
#include "KRef.h"
#include "KRes.h"
#include "KScene.h"
#include "KScreen.h"
#include "KShadow.h"
#include "KSig.h"
#include "KSnapshotter.h"
#include "KSolidBody.h"
#include "KSound.h"
#include "KStorage.h"
#include "KString.h"
#include "KSpriteDrawable.h"
#include "KSystem.h"
#include "KTable.h"
#include "KTextNode.h"
#include "KThread.h"
#include "KUserData.h"
#include "KVec.h"
#include "KVideo.h"
#include "KWindow.h"
#include "KWindowDragger.h"
#include "KWindowResizer.h"
#include "KZlib.h"
#include "KZip.h"
#include "KXml.h"


#ifndef KENG_DISABLE_PRAGMA_LINK
#pragma comment(lib, "winmm.lib")   // mmsystem.h
#pragma comment(lib, "imm32.lib")   // imm.h (by ImGui)
#pragma comment(lib, "shlwapi.lib") // shlwapi.h
#pragma comment(lib, "comctl32.lib")// commctrl.h
#pragma comment(lib, "dbghelp.lib") // dbghelp.h
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "legacy_stdio_definitions.lib") // For compatibility with vsnprintf at dxerr.dll
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxerr9.lib") // DirextX SDK 2004 Oct

// http://springhead.info/wiki/index.php
// Visual Studio 2012でXINPUTにまつわるエラーが出る場合
//
// Window7 以前のOSでの実行で、
// 「コンピュータに XINPUT1_4.dll がないため、プログラムを開始できません。…」
// というエラーが発生する場合は、ビルドのプロパティで、
//   [リンカー]-[入力]-[追加の依存ファイル] に XINPUT9_1_0.LIB を
//   [リンカー]-[入力]-[特定の既定のライブラリの無視] に XINPUT.LIB を
// 指定してください。
// Windows8 以降のOSでは発生しないと思いますが、確認はしていません。
// #pragma comment(lib, "xinput9_1_0.lib")

#endif


// 自力でビルドするときのヒント
// プリプロセッサとして定義するもの
//	_CRT_SECURE_NO_WARNINGS
//	NOMINMAX
//	IMGUI_IMPL_WIN32_DISABLE_GAMEPAD (XInput をリンクさせないため ==> imgui/imgui_impl_win32.cpp)






/** @mainpage Kamilo メモ


@tableofcontents




<hr>
@subsection ss_include インクルードすべきファイル

<ul>
	<li><b>Kamilo.h</b></li>
</ul>
<br>
<br>
<br>




<hr>
@subsection ss_link リンクすべきライブラリ

<ul><li>
	Kamilo.h 内に <code>#pragma comment(lib, "...")</code> で記述済み。
	これを無効にしたい場合は後述の <b>KENG_DISABLE_PRAGMA_LINK</b> を定義しておく
</li></ul>
<br>
<br>
<br>




<hr>
@subsection ss_macro マクロの設定

<ul>
<li><b>_CRT_SECURE_NO_WARNINGS</b><br>
	strcpy などの古い関数で警告が発生しないようにする<br>
</li>
<li><b>NOMINMAX</b><br>
	Windows.h のマクロ min, max が std::min, std::max と競合しないようにする<br>
</li>
<li><b>IMGUI_IMPL_WIN32_DISABLE_GAMEPAD</b><br>
	XInput をリンクさせないために定義する → imgui/imgui_impl_win32.cpp<br>
</li>
<li><b>KENG_DISABLE_PRAGMA_LINK</b><br>
	pragma によるライブラリのリンクを抑制したい場合に使う → Kamilo.h<br>
</li>
</ul>
<br>
<br>
<br>




<hr>
@subsection build コンパイルできることを確認する
<ul>
	<li> main.cpp に以下の内容をそのままコピペする。真っ黒なウィンドウが開けばOK
	@code
		#include <Windows.h>
		#include <Kamilo.h>
		using namespace Kamilo;
		int WINAPI WinMain(HINSTANCE hCurrInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
			KEngineDef def;
			def.resolution_x = 800;
			def.resolution_y = 600;
			if (KEngine::create(&def)) {
				KEngine::run();
				KEngine::destroy();
			}
		}
	@endcode
	</li>
</ul>
<br>
<br>
<br>





<hr>
@subsection ss_error エラー対処
<ul>
<li><b>未解決のシンボル _main が関数 ....</b><br>
	プロジェクトのプロパティページの [構成プロパティ][リンカー][システム][サブシステム] を「Windows」にする
</li>

<li><b>warning C4996: 'strcpy': This function ...</b><br>
	プロジェクトのプロパティページの [構成プロパティ][C/C++][プリプロセッサ] で <b>_CRT_SECURE_NO_WARNINGS</b> を定義しておく
</li>

<li><b>コンピュータに XINPUT1_4.dll がないため、プログラムを開始できません</b><br>
	<b>IMGUI_IMPL_WIN32_DISABLE_GAMEPAD</b> を定義しておく<br>
	あるいはプロジェクトののプロパティページで以下の項目を設定する：<br>
	[リンカー][入力][追加の依存ファイル] に <b>XINPUT9_1_0.LIB</b> を追加<br>
	[リンカー][入力][特定の既定のライブラリの無視] に <b>XINPUT.LIB</b> を追加<br>
</li>
</ul>
<br>
<br>
<br>





<hr>
@subsection ss_thirdparty 同梱ライブラリ

<ul>
	<li>
		<b>stb libraries</b><br>
		Copyright (c) 2017 Sean Barrett<br>
		https://github.com/nothings/stb/blob/master/LICENSE <br>
		<br>
	</li>
	<li>
		<b>miniz</b><br>
		Copyright 2013-2014 RAD Game Tools and Valve Software<br>
		Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC<br>
		https://github.com/richgel999/miniz/blob/master/LICENSE <br>
		<br>
	</li>
	<li>
		<b>Lua</b><br>
		Copyright (C) 1994-2018 Lua.org, PUC-Rio<br>
		https://www.lua.org/license.html <br>
		<br>
	</li>
	<li>
		<b>TinyXml2</b><br>
		TinyXML-2 is released under the zlib license<br>
		https://github.com/leethomason/tinyxml2/blob/master/LICENSE.txt <br>
		<br>
	</li>
	<li>
		<b>Dear ImGUI</b><br>
		Copyright (c) 2014-2020 Omar Cornut<br>
		https://github.com/ocornut/imgui/blob/master/LICENSE.txt <br>
		<br>
	</li>
</ul>
<br>
<br>
<br>
	
	
*/
