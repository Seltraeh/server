#pragma once

// TODO: move this to a view so we can add version checking
//
// The title's Terms and Conditions dialog shows this page in a native web view
// (TitleScene::showTermsAndConditions -> WebViewScene, TERMS_AND_CONDITIONS_URL)
// that is transparent over the dialog art, so the page sets its own colours:
// with the browser defaults the text was black on the dark dialog, and hard to
// read.  The client also places the view over the dialog's caption.  Measured on
// the title's Terms & Conditions dialog (831x934 window, 2026-10-23), the view is
// 522 px wide and its top edge sits 118 px above the caption's separator.  The
// first-launch agreement dialog shows the same page in a SHORTER view (its
// checkbox text takes the bottom), with the same top and width.  So the offset
// and sizes are in vw: 23vw clears the separator in both dialogs, and everything
// scales with the window.
constexpr auto kWebTermsData = R"AADATA(
	<!DOCTYPE html>
	<html>
	<head>
	<meta charset="utf-8">
	<style>
		html, body { margin: 0; height: 100%; background: transparent; }
		body {
			box-sizing: border-box; padding: 23vw 2.9vw 0;
			color: #f2ead6; text-shadow: 0 1px 2px #000;
			font-family: "Segoe UI", Arial, sans-serif; font-size: 2.9vw; line-height: 1.45;
		}
		.panel { padding: 1.9vw 2.9vw; background: rgba(10, 12, 28, 0.72); border-radius: 6px; }
		h1 { margin: 0 0 1vw; color: #ffd772; font-size: 3.8vw; }
		p { margin: 0; }
		a { color: #8fd3ff; }
	</style>
	</head>
	<body>
		<div class="panel">
			<h1>Brave Frontier Offline Mod</h1>
			<p>The offline server is released under AGPL v3.0+, by using this software you agree to the license.<br>
				You can view a copy of the license by clicking <a href="https://github.com/decompfrontier/server/LICENSE">here</a>
			</p>
		</div>
	</body>
	</html>
)AADATA";
