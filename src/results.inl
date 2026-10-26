// App's unified command/query routing. No tabs are needed to reach any feature.
void rebuild(bool preserveSelection = true) {
    std::optional<Row> previousRow;
    if (preserveSelection && selected >= 0 && selected < int(rows.size())) previousRow = rows[selected];
    rows.clear();
    conversionCard=false;
    auto q = trim(query);
    resultMode = mode;
    auto prefix = [&](const wchar_t* word, Mode route) {
        std::wstring start = std::wstring(word) + L" ";
        if (lower(q).starts_with(start)) {
            q = trim(q.substr(start.size()));
            resultMode = route;
            return true;
        }
        return false;
    };
    if (mode == Mode::Home) {
        if (!prefix(L"emoji", Mode::Emoji) && !prefix(L"emojis", Mode::Emoji) &&
            !prefix(L"clipboard", Mode::Clipboard) && !prefix(L"clip", Mode::Clipboard) &&
            !prefix(L"history", Mode::Clipboard) && !prefix(L"open", Mode::Apps) &&
            !prefix(L"apps", Mode::Apps)) {
            prefix(L"convert", Mode::Convert);
        }
    }
    const auto normalizedQuery = lower(q);

    if ((resultMode == Mode::Home || resultMode == Mode::Convert) && !q.empty()) {
        if (auto calculation = calculate(q)) {
            rows.push_back({calculation->answer, calculation->error ? L"Check the expression" : q + L" · Enter to copy", L"calculator",
                Action::Copy, 0, calculation->error ? L"" : calculation->answer, 20000});
        } else if (auto units = convert_units(q)) {
            rows.push_back({units->answer, units->detail, L"calculator", Action::Copy, 0, units->answer, 20000});
        } else if (auto time = convert_time(q)) {
            rows.push_back({time->answer, time->detail, L"clock-3", Action::Copy, 0, time->answer, 20000});
        } else if (auto currency = parse_currency(q,localCurrency)) {
            auto from = rates.find(currency->from), to = rates.find(currency->to);
            bool haveFrom = currency->from == L"USD" || from != rates.end();
            bool haveTo = currency->to == L"USD" || to != rates.end();
            if (currency->from == currency->to) {
                rows.push_back({currency->amount + L" " + currency->to, L"Same currency", L"arrow-left-right", Action::Copy, 0, currency->amount, 20000});
            } else if (haveFrom && haveTo) {
                int places = currency->to == L"JPY" || currency->to == L"KRW" ? 0 : 2;
                auto answer = currency_cross_result(currency->amount,
                    currency->from == L"USD" ? L"1" : from->second.value,
                    currency->to == L"USD" ? L"1" : to->second.value, places);
                std::wstring date = currency->from == L"USD" ? to->second.date : from->second.date;
                if (currency->from != L"USD" && currency->to != L"USD" && from->second.date != to->second.date)
                    date = from->second.date + L" / " + to->second.date;
                if (answer) {
                    rows.push_back({*answer + L" " + currency->to,
                        currency->amount + L" " + currency->from + L" · Frankfurter · " + date +
                        (rateFetchedOn != today() ? L" · cached" : L""),
                        L"arrow-left-right", Action::Copy, 0, *answer, 20000});
                } else {
                    rows.push_back({L"Amount exceeds supported precision", L"Use up to 12 digits, including decimal digits",
                        L"calculator", Action::Copy, 0, L"", 20000});
                }
            } else {
                rows.push_back({rateBusy ? L"Updating rates…" : L"Rate unavailable",
                    rateBusy ? L"" : (rateError.empty() ? L"Run Refresh Rates to retry" : rateError),
                    L"arrow-left-right", Action::Copy, 0, L"", 20000});
            }
        }
    }

    conversionCard=!rows.empty()&&rows.front().score==20000;
    if (resultMode == Mode::Home || resultMode == Mode::Apps) {
        if (q.empty()) {
            const bool home = resultMode == Mode::Home;
            const auto frequent = frequent_app_indices(apps, home ? 5 : 100, !home);
            for (size_t n = 0; n < frequent.size(); ++n) {
                const auto index = frequent[n];
                const auto& item = apps[index];
                rows.push_back({item.name, L"", L"app-window", Action::App, int(index),
                    item.target, 1000 - int(n), 0,
                    n < 5 && item.uses > 0 ? L"Frequently Used" : L"Applications"});
            }
        } else {
            struct Match { int score; size_t index; };
            std::vector<Match> matches;
            matches.reserve(apps.size());
            for (size_t i = 0; i < apps.size(); ++i) {
                auto& item = apps[i];
                int score = fuzzy_score_normalized(normalizedQuery, item.search);
                if (score)
                    matches.push_back({score + std::min(item.uses, 60) * 5, i});
            }
            // Rank cheap indices first; construct strings for at most 100 results.
            const size_t count = std::min<size_t>(matches.size(), 100);
            std::partial_sort(matches.begin(), matches.begin() + count, matches.end(), [](auto a, auto b) {
                return a.score != b.score ? a.score > b.score : a.index < b.index;
            });
            for (size_t n = 0; n < count; ++n) {
                auto [score, index] = matches[n]; auto& item = apps[index];
                rows.push_back({item.name, L"", L"app-window", Action::App, int(index), item.target, score});
            }
        }
    }

    if (resultMode == Mode::Home) {
        struct Feature { const wchar_t* name; const wchar_t* aliases; const wchar_t* detail; const wchar_t* icon; Action action; int index; };
        const Feature features[] = {
            {L"Clipboard History", L"clipboard history paste copied text", L"Search Clipboard History", L"clipboard", Action::Mode, int(Mode::Clipboard)},
            {L"Emoji", L"emoji emojis symbols smile search emoji", L"", L"smile", Action::Mode, int(Mode::Emoji)},
            {L"Camera", L"camera webcam mirror snapshot", L"Webcam Mirror", L"camera", Action::Mode, int(Mode::Camera)},
            {L"Calculator & Converter", L"calculator convert conversion units currency time", L"Type a calculation or conversion", L"calculator", Action::Mode, int(Mode::Convert)},
            {L"Applications", L"apps applications launcher search applications", L"", L"app-window", Action::Mode, int(Mode::Apps)},
            {L"Settings", L"settings preferences hotkeys shortcuts", L"Configure Minicast", L"settings-2", Action::Settings, 0},
            {L"Refresh Rates", L"refresh rates currency exchange update currencies", L"", L"refresh-cw", Action::RefreshRates, 0},
            {L"Refresh Apps", L"refresh apps rescan index", L"", L"refresh-cw", Action::RefreshApps, 0},
            {L"Check for Updates", L"check updates update version release download", L"Open the release page", L"refresh-cw", Action::Updates, 0},
            {L"About Minicast", L"about minicast version", L"Minicast " MINICAST_VERSION_WSTRING, L"app-window", Action::About, 0}
        };
        for (size_t i = 0; i < std::size(features); ++i) {
            const auto& feature = features[i];
            if (q.empty() && i >= 6) continue;
            int score = std::max(fuzzy_score(q, feature.name), fuzzy_score(q, feature.aliases));
            if (!score && conversionCard && (i==0||i==1||i==2||i==5))score=1;
            if (score) rows.push_back({feature.name, feature.detail, feature.icon, feature.action,
                feature.index, L"", score + (q.empty() ? 10 : 20), 0, q.empty() ? L"Commands" : L""});
        }
    }

    if (resultMode == Mode::Convert && q.empty()) {
        rows = {
            {L"(125 + 75) * 1.1", L"Arithmetic, parentheses, powers and percentages", L"calculator", Action::Query},
            {localCurrency.empty()?L"100 usd to eur":localCurrency==L"USD"?L"100 eur":L"100 usd",
                localCurrency.empty()?L"Currency":L"Currency · "+localCurrency, L"arrow-left-right", Action::Query},
            {L"25 c to f", L"Temperature, length, mass, speed and storage", L"calculator", Action::Query},
            {L"9am Jakarta to London tomorrow", L"Time zones and daylight-saving rules", L"clock-3", Action::Query},
            {L"time Tokyo", L"Current local time in another city", L"clock-3", Action::Query},
            {L"2 gb to mib", L"Decimal and binary storage units", L"calculator", Action::Query}
        };
    }

    if (resultMode == Mode::Emoji) {
        static const auto searchNames = [] {
            std::vector<std::pair<std::wstring, std::wstring>> names;
            for (auto& item : emojis()) names.emplace_back(lower(item.name), lower(item.keywords));
            return names;
        }();
        int i = 0;
        for (auto& item : emojis()) {
            if(emojiCategory>=0&&int(item.category)!=emojiCategory){++i;continue;}
            int score = std::max(fuzzy_score_normalized(normalizedQuery, searchNames[i].first),
                fuzzy_score_normalized(normalizedQuery, searchNames[i].second));
            if (score) rows.push_back({item.name, L"", item.glyph,
                Action::Copy, i, item.glyph, score,0,q.empty()?std::wstring(emoji_category_name(item.category)):L"Results"});
            ++i;
        }
        if(q.empty()) {
            std::vector<std::wstring> frequent;
            if(emojiCategory<0){
                std::vector<std::pair<int,std::wstring>> used;
                for(auto& row:rows){auto found=appUsage.find(L"emoji:"+row.value);if(found!=appUsage.end())used.emplace_back(found->second,row.value);}
                std::stable_sort(used.begin(),used.end(),[](const auto& a,const auto& b){return a.first>b.first;});
                for(size_t n=0;n<std::min<size_t>(16,used.size());++n)frequent.push_back(used[n].second);
            }
            for(auto& row:rows){auto found=std::find(frequent.begin(),frequent.end(),row.value);if(found!=frequent.end()){row.score=1000-int(found-frequent.begin());row.group=L"Frequently Used";}else row.score=100-int(emojis()[row.index].category);}
        }
    }

    if (resultMode == Mode::Clipboard) {
        if(historyLoaded)expire_history();
        if (!historyEnabled) {
            rows.push_back({L"Enable Clipboard History", L"Text and images · "+std::to_wstring(historyRetentionDays)+L" days", L"clipboard", Action::EnableHistory});
        } else {
            int i = 0;
            for (auto& item : history) {
                if ((clipboardFilter!=1||item.dib.empty()) && (clipboardFilter!=2||!item.dib.empty()) && contains_folded(item.text, normalizedQuery)) {
                    // Only the visible excerpt belongs to a result; copy the
                    // complete entry once, on activation, from the history store.
                    auto title = item.text.substr(0, 160);
                    if(!item.dib.empty()){BITMAPINFOHEADER h{};memcpy(&h,item.dib.data(),sizeof(h));title=L"Image · "+std::to_wstring(h.biWidth)+L" × "+std::to_wstring(-h.biHeight);}
                    std::replace(title.begin(), title.end(), L'\n', L' ');
                    std::replace(title.begin(), title.end(), L'\r', L' ');
                    std::replace(title.begin(), title.end(), L'\t', L' ');
                    if (item.text.size() > 160) title += L"…";
                    rows.push_back({title, L"", L"clipboard", Action::Copy, i, L"", 0, item.timestamp});
                }
                ++i;
            }
        }
    }

    if (resultMode != Mode::Clipboard) std::stable_sort(rows.begin(), rows.end(), [](auto& a, auto& b) { return a.score > b.score; });
    if (rows.size() > 100 && resultMode != Mode::Clipboard && resultMode != Mode::Emoji) rows.resize(100);
    if (previousRow) {
        auto found = std::find_if(rows.begin(), rows.end(), [&](const Row& row) {
            if (row.action != previousRow->action) return false;
            if (row.identity || previousRow->identity) return row.identity == previousRow->identity;
            if (row.action == Action::App) return row.value == previousRow->value;
            return row.index == previousRow->index && row.title == previousRow->title;
        });
        selected = found == rows.end() ? 0 : int(found - rows.begin());
    }
    selected = std::clamp(selected, 0, std::max(0, int(rows.size()) - 1));
    build_grid();
    if(preserveSelection&&viewportScrolled){
        scroll=std::clamp(scroll,0.f,maximum_list_scroll());
    }else ensure_selection_visible();
    position_edit();
    hover = -1;
    if (resultMode == Mode::Clipboard) status = historyEnabled ? std::to_wstring(history.size()) + L" items" : L"History is off";
    else if (rateBusy) status = L"Updating rates…";
    else if (indexing) status = L"Updating apps…";
    else status = L"";
    invalidate();
}
