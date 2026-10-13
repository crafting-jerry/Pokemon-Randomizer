// Gemeinsames Grundgeruest der HTML-Spoiler-Logs
#include "headers/common/spoiler_html.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace spoiler {

namespace {
const char* const kStyle = R"(
:root{--bg:#0f1117;--panel:#171a23;--card:#1e2230;--line:#2a2f40;--text:#e8eaf0;--muted:#8b91a5;--accent:#8b5cf6}
*{box-sizing:border-box}body{margin:0;font-family:"Segoe UI",system-ui,sans-serif;background:var(--bg);color:var(--text)}
.top{position:sticky;top:0;z-index:5;background:rgba(15,17,23,.95);backdrop-filter:blur(6px);border-bottom:1px solid var(--line);padding:16px 24px}
.top h1{margin:0 0 4px;font-size:22px}.top .info{color:var(--muted);font-size:13px;margin-bottom:12px}
.controls{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
#search{flex:1;min-width:220px;padding:10px 14px;border-radius:10px;border:1px solid var(--line);background:var(--panel);color:var(--text);font-size:15px}
#search:focus{outline:2px solid var(--accent);border-color:transparent}
.chip{padding:7px 12px;border-radius:999px;border:1px solid var(--line);background:var(--panel);color:var(--text);cursor:pointer;font-size:13px}
.chip.active{background:var(--accent);border-color:var(--accent)}
main{padding:20px 24px;max-width:1500px;margin:0 auto}
details.section{margin-bottom:18px;background:var(--panel);border:1px solid var(--line);border-radius:14px}
details.section>summary{cursor:pointer;padding:14px 18px;font-size:18px;font-weight:600;list-style:none}
details.section>summary::-webkit-details-marker{display:none}
details.section>summary .count{color:var(--muted);font-weight:400;font-size:14px;margin-left:8px}
.section-body{padding:0 18px 18px}
.trainer{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px;margin-top:14px}
.trainer h3{margin:0;font-size:16px;display:flex;gap:8px;align-items:center}.trainer .sub{color:var(--muted);font-size:12px;margin-top:3px}
.team{display:grid;grid-template-columns:repeat(auto-fill,minmax(200px,1fr));gap:10px;margin-top:12px}
.mon{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:10px}
.mon-head{display:flex;align-items:baseline;gap:6px}.mon-name{font-weight:600;flex:1}
.lvl{color:var(--muted);font-size:13px}.shiny{color:#facc15}
.types{margin:6px 0;display:flex;gap:4px;flex-wrap:wrap}
.type{display:inline-block;padding:2px 8px;border-radius:6px;font-size:11px;font-weight:600;color:#fff;text-shadow:0 1px 1px rgba(0,0,0,.4)}
.gmax{display:inline-block;padding:2px 8px;border-radius:6px;font-size:11px;font-weight:600;color:#fff;background:linear-gradient(90deg,#e11d48,#9333ea)}
.meta{font-size:12px;margin-top:4px;color:#cfd3df}
.moves{margin:8px 0 0;padding-left:16px;font-size:13px}.moves li{margin:2px 0}
.hidden{display:none!important}
.t0{background:#9fa19f}.t1{background:#ff8000}.t2{background:#81b9ef}.t3{background:#9141cb}.t4{background:#915121}
.t5{background:#afa981}.t6{background:#91a119}.t7{background:#704170}.t8{background:#60a1b8}.t9{background:#e62829}
.t10{background:#2980ef}.t11{background:#3fa129}.t12{background:#d4b000}.t13{background:#ef4179}.t14{background:#3dcef3}
.t15{background:#5060e1}.t16{background:#624d4e}.t17{background:#ef70ef}
#empty{color:var(--muted);text-align:center;padding:40px}
.section-body>.team{margin-top:4px}
table.enc{width:100%;border-collapse:collapse;font-size:14px}
table.enc th{text-align:left;color:var(--muted);font-weight:600;font-size:12px;padding:6px 8px;border-bottom:1px solid var(--line)}
table.enc td{padding:6px 8px;border-bottom:1px solid var(--line);vertical-align:middle}
table.enc td .type{margin-left:4px}
table.enc .num{color:var(--muted);white-space:nowrap}.arrow{color:var(--muted)}
.hint{color:var(--muted);font-size:12px}
.wild{display:inline-flex;align-items:center;gap:5px;background:var(--card);border:1px solid var(--line);border-radius:999px;padding:2px 9px;margin:2px 2px;font-size:13px;cursor:default}
.dot{width:8px;height:8px;border-radius:50%;display:inline-block}
td.place{white-space:nowrap;font-weight:600}
)";

const char* const kScript = R"(
(function(){
  var sections=[].slice.call(document.querySelectorAll('details.section'));
  var search=document.getElementById('search');var chips=document.getElementById('chips');var activeCats=new Set();
  sections.forEach(function(s){
    var b=document.createElement('button');b.className='chip';b.textContent=s.dataset.cat;
    b.onclick=function(){if(activeCats.has(s.dataset.cat)){activeCats.delete(s.dataset.cat);b.classList.remove('active');}else{activeCats.add(s.dataset.cat);b.classList.add('active');}apply();};
    chips.appendChild(b);
  });
  function apply(){
    var q=search.value.trim().toLowerCase();var terms=q?q.split(/\s+/):[];var any=false;
    sections.forEach(function(s){
      var catOk=activeCats.size===0||activeCats.has(s.dataset.cat);var visible=0;
      s.querySelectorAll('[data-search]').forEach(function(t){
        var ok=catOk&&terms.every(function(w){return t.dataset.search.indexOf(w)!==-1;});
        t.classList.toggle('hidden',!ok);if(ok)visible++;
      });
      s.classList.toggle('hidden',visible===0);if(visible>0)any=true;if(terms.length&&visible>0)s.open=true;
    });
    document.getElementById('empty').classList.toggle('hidden',any);
  }
  search.addEventListener('input',apply);
  document.getElementById('expand').onclick=function(){sections.forEach(function(s){s.open=true;});};
  document.getElementById('collapse').onclick=function(){sections.forEach(function(s){s.open=false;});};
})();
)";
} // namespace

QString head(const QString& pageTitle, const QString& gameTitle, const QString& info) {
    QString html = R"(<!DOCTYPE html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Spoiler-Log · )" + pageTitle.toHtmlEscaped() + "</title>\n<style>" + QString::fromUtf8(kStyle) + "</style></head><body>\n";
    html += "<div class=\"top\"><h1>Spoiler-Log · " + gameTitle.toHtmlEscaped() + "</h1>";
    html += "<div class=\"info\">" + info + "</div>";
    html += R"(<div class="controls"><input id="search" type="search" placeholder="Suchen: Trainer, Pokémon, Attacke, Item …" autocomplete="off">
<button class="chip" id="expand">Alle aufklappen</button><button class="chip" id="collapse">Alle zuklappen</button></div>
<div class="controls" id="chips" style="margin-top:10px"></div></div><main>)";
    return html;
}

QString foot() {
    return R"(<div id="empty" class="hidden">Keine Treffer.</div></main>
<script>)" + QString::fromUtf8(kScript) + "</script></body></html>";
}

bool write(const QString& path, const QString& html) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << html;
    return true;
}

} // namespace spoiler
