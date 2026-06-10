# Tool Catalogue

55 tools across 10 categories. All tools are Python scripts executed by the C++ MCP server as subprocesses. API keys are injected securely at runtime — never stored in tool scripts.

---

## Category: `web_search`

| Tool | Description | Required API Key(s) |
|------|-------------|---------------------|
| `web_search` | Google Custom Search — returns titles, URLs, and snippets | `GOOGLE_API_KEY`, `GOOGLE_CSE_ID` |
| `web_search_images` | Google image search | `GOOGLE_API_KEY`, `GOOGLE_CSE_ID` |
| `web_fetch_page` | Fetch and strip HTML from any URL to plain text | none |
| `web_search_news` | Recent news via Google Custom Search | `GOOGLE_API_KEY`, `GOOGLE_CSE_ID` |
| `web_search_scholar` | Academic papers via Semantic Scholar (free, no key) | none |

---

## Category: `maps`

Uses the Google Maps Platform APIs.

| Tool | Description |
|------|-------------|
| `maps_geocode` | Address → lat/lng |
| `maps_reverse_geocode` | lat/lng → address |
| `maps_directions` | Turn-by-turn directions (driving/walking/cycling/transit) |
| `maps_nearby_places` | Find nearby restaurants, hospitals, shops, etc. |
| `maps_place_details` | Detailed info for a place (hours, phone, website, rating) |
| `maps_distance_matrix` | Distances and travel times for multiple origins/destinations |
| `maps_timezone` | Timezone ID and offset for a coordinate |
| `maps_elevation` | Elevation in metres for a coordinate |

**Required key:** `GOOGLE_API_KEY`

---

## Category: `weather`

Uses OpenWeatherMap APIs.

| Tool | Description |
|------|-------------|
| `weather_current` | Current temperature, humidity, wind, description |
| `weather_forecast` | 5-day / 3-hour interval forecast |
| `weather_air_quality` | AQI and pollutant concentrations (CO, NO₂, PM2.5, etc.) |
| `weather_uv_index` | Current UV index |
| `weather_alerts` | Severe weather alerts (One Call API 3.0) |

**Required key:** `OPENWEATHER_API_KEY`

---

## Category: `news`

Uses NewsAPI.org.

| Tool | Description |
|------|-------------|
| `news_top_headlines` | Top headlines by country and category |
| `news_search` | Full-text article search across all sources |
| `news_sources` | List available publishers/sources |

**Required key:** `NEWSAPI_KEY`

---

## Category: `finance`

Uses Alpha Vantage.

| Tool | Description |
|------|-------------|
| `finance_stock_quote` | Real-time quote (price, change, volume) |
| `finance_stock_history` | Daily adjusted OHLCV history |
| `finance_currency_exchange` | FX rates between any two currencies |
| `finance_crypto_price` | Crypto price vs fiat (BTC/USD, ETH/EUR, etc.) |
| `finance_company_info` | Fundamentals: P/E, market cap, sector, EPS |

**Required key:** `ALPHA_VANTAGE_KEY`

---

## Category: `github`

Uses the GitHub REST API. Authenticate with `GITHUB_TOKEN` for higher rate limits (optional).

| Tool | Description |
|------|-------------|
| `github_search_repos` | Search repositories by keyword, language, stars |
| `github_get_repo` | Full repo details: stars, forks, license, topics |
| `github_list_issues` | List open/closed issues |
| `github_search_code` | Search code across all public repos |
| `github_list_prs` | List pull requests |
| `github_get_user` | Public profile: bio, repos, followers |
| `github_list_commits` | Recent commit history with messages |
| `github_get_file` | Read a file from any branch/tag/commit |

---

## Category: `translation`

| Tool | API | Description |
|------|-----|-------------|
| `translate_text` | Google Translate | Translate text to any of 130+ languages |
| `translate_detect_language` | Google Translate | Auto-detect the language of text |
| `translate_supported_languages` | Google Translate | List all supported languages |
| `deepl_translate` | DeepL | High-quality neural translation (EU/US data centres) |

**Required keys:** `GOOGLE_API_KEY` for Google tools, `DEEPL_API_KEY` for DeepL

---

## Category: `youtube`

Uses the YouTube Data API v3.

| Tool | Description |
|------|-------------|
| `youtube_search` | Search videos by keyword with sorting options |
| `youtube_get_video` | Stats, duration, full description for a video |
| `youtube_list_channel` | Most recent uploads from a channel |

**Required key:** `YOUTUBE_API_KEY` (or `GOOGLE_API_KEY`)

---

## Category: `knowledge`

| Tool | API | Description |
|------|-----|-------------|
| `wikipedia_search` | Wikipedia | Search articles by keyword |
| `wikipedia_get_summary` | Wikipedia | Article introduction (REST v1) |
| `wikipedia_get_article` | Wikipedia | Full article text (up to 8 000 chars) |
| `wikipedia_get_related` | Wikipedia | Internal links (related topics) |
| `wolfram_query` | Wolfram Alpha | Short answer — math, science, units, dates |
| `wolfram_full_query` | Wolfram Alpha | Multi-pod detailed computation |

Wikipedia tools require no API key. **Required keys:** `WOLFRAM_APP_ID` for Wolfram Alpha.

---

## Category: `nlp`

Uses Google Cloud Natural Language API.

| Tool | Description |
|------|-------------|
| `google_nlp_sentiment` | Document-level sentiment score and magnitude |
| `google_nlp_entities` | Named entities with type, salience, and Wikipedia links |

**Required key:** `GOOGLE_API_KEY` (must have NLP API enabled)

---

## Category: `vision`

| Tool | API | Description |
|------|-----|-------------|
| `google_vision_labels` | Google Vision | Detect objects, scenes, and concepts in an image |

**Required key:** `GOOGLE_API_KEY` (must have Vision API enabled)

---

## Category: `images`

| Tool | API | Description |
|------|-----|-------------|
| `unsplash_search` | Unsplash | Search royalty-free photos |
| `unsplash_random` | Unsplash | Get a random photo, optionally filtered by topic |

**Required key:** `UNSPLASH_ACCESS_KEY`

---

## Category: `utilities`

No external API keys required.

| Tool | Description |
|------|-------------|
| `util_url_encode` | URL-encode or URL-decode a string |
| `util_base64` | Base64 encode/decode (standard and URL-safe variants) |
| `util_hash_compute` | MD5, SHA-1, SHA-256, SHA-512 (hex or base64 output) |
| `util_json_validate` | Validate and pretty-print JSON |
| `util_regex_match` | Test a regex — findall / match / search modes |
| `util_time_convert` | Convert timestamps between ISO 8601, unix epoch, and timezones |
| `util_ip_info` | Geolocation, ASN, and hostname for an IP (ipinfo.io) |
| `util_markdown_to_html` | Convert Markdown to HTML (no external dependencies) |

---

## Adding a New Tool

1. Create `tools/<tool_name>.py` following the `_base.py` protocol:
   ```python
   import sys, os; sys.path.insert(0, os.path.dirname(__file__))
   from _base import run

   def handler(args, api_keys):
       # ... call API, return result ...
       return {"result": "value"}

   run(handler)
   ```

2. Add an entry to `tools/tools_manifest.json`:
   ```json
   {
     "name": "my_tool",
     "category": "my_category",
     "script": "my_tool.py",
     "description": "What this tool does.",
     "inputSchema": {
       "type": "object",
       "required": ["param1"],
       "properties": {
         "param1": {"type": "string", "description": "..."}
       }
     }
   }
   ```

3. If your tool needs an API key, add the env var name to the `API_KEY_VARS` list in `src/tool_runner.cpp` and document it in `.env.example`.

4. Restart the MCP server — it reloads the manifest on startup.
