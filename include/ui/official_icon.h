#pragma once
// Miniatura 48x48 da arte oficial do ícone NRO usada apenas na tela Sobre.
// A versão principal de alta resolução continua em assets/icon.jpg.
namespace sfm::ui::brand {
inline constexpr int kIconSize = 48;
inline constexpr const char* kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
inline constexpr const char* kPaletteHex = "f1f6fbe8e3c1eac461fcc305bbd4dab9bebf22dcfa0ebaf7c6aa6ed28c33979a9c8b88896e8293268ac80a9adf0b7fd1e04f53e137387f68758b40515f69755357604a4a53513c4a1f679d0567c82b4d6d074d9c393b45203a57043ba10b386af22420e31014b22127b8080e751b234b1b1e690408450305272831142742241a1e1618212d0a102f0105150f16170406022b7702216605234d031b4202184401133f041329060f21020b28070a10020313030307040302050000000002000000";
inline constexpr const char* kRows[kIconSize] = {
"///88+/tkiRRRRRRRRRRRRiRQTSSLLMMMNNNHHNY2/977///",
"///7+viQQikllnnnsvvuvqXXqlluuuuuryyyfYNGGY2/7+//",
"+/7/mQIinttnmmkv+757oKUFUhgk66676xx0144yYEGf98+/",
"88+kIQmtnmmjhRl5uur7LSLUShhhs32462bewwx01xNGf/7/",
"8+lIQntnmjjhRkvurruuLVFLShhhs42223yPZeewy11NGf98",
"+tQQntmmjhgQTtsrrr35Ku/VSjnjs2221z4fOPZewyz1NG3+",
"/iQntnmjhhilnsqrru35Ku8USjkhs222zzz1fYPZewx1xGY/",
"tQktnmjmmnv99765u3r7KW/UShhhr222214246zfeewy4YH3",
"kRnnmjnvtvlKFFFU+337qKLFXjjn32446WFo62335fex0yHf",
"RitmjmtsvqAELLKVu57r+/oquvKUu6rouMAoo33222ewy1OY",
"gmnmjtss9WAFUWuoAoWArUFKFLAAUKAALLAEAFr424yex1ZO",
"gmnjmvsvv5LFAAAVAUFAVASFFcAcUAcq5FEoLAo6222ww1bO",
"hmmktssu7qoqoLAWFAKKAErELWA5KE775EL/LA331z1yeybO",
"hmknnntv9FAEEEL/FArVAU5AWcAKWEEEVEV/KF7441yyexbO",
"hmmnmkXXocWWWcqocWocVooVcoWWrcVVcWooWXucWaxxxwbO",
"hmnkLEGGGaWccWWWccWWcWWcWWccWcccWcXWXWTIIILaxxbP",
"hnnSGGGHOa9+6666636441111110yyyxxxxx48TiRRQQW1fP",
"hnkEGGHHHav62221zydUMMMLYZbZZZZPOPZZy5TRRTQRQ1fP",
"hnTGHUVYHav6121zzyIDDDDDCMNNNbbZPOPZ05XQQliQQcxP",
"hnSGYlcoHav311zyyfJDDBBBEBEEAKaeeZOZ05TTXgTkRXxP",
"htMGOpufHav300yxxdDCBAAAAAAAEFJUwZOe08XRiTiiRTxP",
"hsFGHOPHHa93yyxwedCAAAABBBBBBIJTwZPe08XRRkiRRTyP",
"hoEGHHNHHa94yxwZNICCCCDDDDDDDDDXwPZe18kggRRggQyZ",
"hcEHHNoOHa92xePOIDDDDDCCCCDDDDDcePew48kgiVXigQdZ",
"jWGHaYHcYa91eOHbJDDDDBICBACDDDJpeZwx48kglcokgQaZ",
"jUGHPYYYOa9yZHPxSDDDCCCCBACDDDJybewx48lgk5ujgRVZ",
"jMGHHPdHHd9wOObxWDDDCJJIIICDDDJxewww28lggjjggRSZ",
"jMHHHHHYOd9wPPexcDDDJIJIBACDDDTxwwwx48lgTgggggSZ",
"jMOOHHHYOd9xPOewdDDDDCCICBJDDDTxwxx03+lhkgggghSZ",
"haNZHHHHHd9xZOPZwJJJJJJJJJJJJJcwwxy168lgggggjRVZ",
"hrYbPHHHHd81wfbfx0ppdddddpz2231z144677lgggghjipP",
"hs1YbZPZZpuu5uqqqquurr5uqqrrqqqqqqqqqqlhhhhjisyP",
"hs66pzVMUWVWV7+755cc55Wo/+57/+5++75//79tnnnt7/yP",
"hs65/cALKSLKKuUc57EA/SAVWVrcUVrWVouUVroV5oWc78fP",
"hn5u+LALVVFKKFKAouEALEBcKBLEKAVSEFFKEKFFFFFV67fP",
"hn7v9KFSVLKKKESKucELASFKKFKF5FKKKFEWFFFLLEr/36fO",
"hk5vsBW//KSKXKKU/WK8uXLSKKSLrKUKKUVFFcKKVK5334YP",
"jittqVqsssvuv9ssllkTTTTSSTTkkkmn9oKKV/6u5563ywNZ",
"mgmmvvtnnttttlkkTXkXXllqqqqqsssur3u572z22430eeHb",
"ngjjnsnTTTTloUcooupaddppdUaddpdYadapaYYYYy2wePGy",
"9jRmmvlXLKKlcp5UFcaaUMVdayVJpfayaMdfa4MM0a1weGO6",
"+nRimtsXFFFlorKEMopVLMMppdaJJ1pVYYVppoSSoyyeHGf9",
"88jQinllEEFoqMEKr3p3VU2zpaYYMpdaMKadzJJJTfxPGP7+",
"8+thQinXLLLXqVMpzdpyfaxfpfZZedp2VUpdyXTTabNGO29+",
"/97thQikllllvqopdo3daYapzdaaap2yddappfffYNGOz/7+",
"///+tjQQkmntnttvvvr21z0yxyx0000011100yfYHHZ198+/",
"///8+9niRQQRiiiTTSSMMMNNNNNNNNNNNNNNNOHOZy797+//",
"///99879nmjiiiiiTTTSUUUYYYZZPPPPPZYYYbf179/++///"
};
}
