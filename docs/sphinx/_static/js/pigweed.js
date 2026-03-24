// Copyright 2025 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

window.pw = {};

// Display inline search results under the search modal. After the user types
// text in the search box, results are shown underneath the text input box.
// The search is restarted after each keypress.
//
// TODO: b/363034219 - Try to upstream this code into pydata-sphinx-theme.
window.pw.initSearch = () => {
  // Don't interfere with the default search UX on /search.html.
  if (window.location.pathname.endsWith('/search.html')) {
    return;
  }
  // The template //docs/sphinx/layout/page.html ensures that Search is always
  // loaded at this point.
  // eslint-disable-next-line no-undef
  if (!Search) {
    return;
  }
  // Destroy the previous search container and create a new one.
  window.pw.resetSearchResults();
  let timeoutId = null;
  let lastQuery = '';
  const searchInput = document.querySelector('#search-input');
  // Set up the event handler to initiate searches whenever the user
  // types stuff in the search modal textbox.
  searchInput.addEventListener('keyup', () => {
    const query = searchInput.value;
    // Don't search when there's nothing in the query textbox.
    if (query === '') {
      return;
    }
    // Don't search if there is no detectable change between
    // the last query and the current query. E.g. user presses
    // Tab to start navigating the search results.
    if (query === lastQuery) {
      return;
    }
    // The user has changed the search query. Delete the old results
    // and start setting up the new container.
    window.pw.resetSearchResults();
    // Debounce so that the search only starts only when the
    // user stops typing.
    const delay_ms = 500;
    lastQuery = query;
    if (timeoutId) {
      window.clearTimeout(timeoutId);
    }
    timeoutId = window.setTimeout(() => {
      // eslint-disable-next-line no-undef
      Search.performSearch(query);
      timeoutId = null;
    }, delay_ms);
  });
};

// Deletes the old custom search results container and recreates a
// new, empty one.
//
// Note that Sphinx assumes that searches are always made from /search.html
// so there's some safeguard logic to make sure the inline search always
// works no matter what pigweed.dev page you're on. b/365179592
//
// TODO: b/363034219 - Try to upstream this code into pydata-sphinx-theme.
window.pw.resetSearchResults = () => {
  let results = document.querySelector('#search-results');
  if (results) {
    results.remove();
  }
  results = document.createElement('section');
  results.classList.add('pw-search-results');
  results.id = 'search-results';
  // Add the new results container to the DOM.
  let modal = document.querySelector('.search-button__search-container');
  modal.appendChild(results);
  // Relative path to the root directory of the site. Sphinx's
  // HTML builder auto-inserts this metadata on every page.
  const root = document.documentElement.dataset.content_root;
  // As Sphinx populates the search results, this observer makes sure that
  // each URL is correct (i.e. doesn't 404). b/365179592
  const linkObserver = new MutationObserver(() => {
    const links = Array.from(
      document.querySelectorAll('#search-results .search a'),
    );
    // Check every link every time because the timing of when new results are
    // added is unpredictable and it's not an expensive operation.
    links.forEach((link) => {
      // Don't use the link.href getter because the browser computes the href
      // as a full URL. We need the relative URL that Sphinx generates.
      const href = link.getAttribute('href');
      if (href.startsWith(root)) {
        // No work needed. The root has already been prepended to the href.
        return;
      }
      link.href = `${root}${href}`;
    });
  });
  // The node that linkObserver watches doesn't exist until the user types
  // something into the search textbox. resultsObserver just waits for
  // that container to exist and then registers linkObserver on it.
  let isObserved = false;
  const resultsObserver = new MutationObserver(() => {
    if (isObserved) {
      return;
    }
    const container = document.querySelector('#search-results .search');
    if (!container) {
      return;
    }
    linkObserver.observe(container, { childList: true });
    isObserved = true;
  });
  resultsObserver.observe(results, { childList: true });
};

// Miscellaneous fixes that are only applied when someone is locally
// previewing the site or viewing the staging site.
window.pw.dev = () => {
  if (window.location.host === 'pigweed.dev') {
    return;
  }
  // Fix the hard-coded C/C++ API reference links. By default they point
  // to the production site. We want to update them to point to the local
  // or staging site.
  const selector = 'ul.nav a.reference.external';
  const links = Array.from(document.querySelectorAll(selector));
  const prefix = 'https://pigweed.dev/api/cc/';
  links.forEach((link) => {
    if (!link.href.startsWith(prefix)) {
      return;
    }
    const target = link.href.replace(prefix, '');
    let tokens = window.location.href.split('/');
    switch (window.location.hostname) {
      case 'localhost':
      case '0.0.0.0':
        link.href = link.href.replace(
          'https://pigweed.dev',
          window.location.origin,
        );
        break;
      case 'storage.googleapis.com':
        // Staging URLs look like this:
        // https://storage.googleapis.com/pigweed-docs-try/8706711556001999761/index.html
        // `tokens` already holds an array like this:
        // ['https:', '', 'storage.googleapis.com', 'pigweed-docs-try', '8706711556001999761', 'index.html']
        // We only need the first 5 tokens.
        tokens.length = 5;
        tokens.push('doxygen');
        tokens = tokens.concat(target.split('/'));
        link.href = tokens.join('/');
        break;
    }
  });
};

window.pw.monkeyPatchSphinxSearchIndex = async () => {
  // eslint-disable-next-line no-undef
  if (!Search) {
    return;
  }
  // eslint-disable-next-line no-undef
  const index = Search._index;
  const objnames = index.objnames;
  const docnames = index.docnames;
  const filenames = index.filenames;
  const titles = index.titles;
  const objects = index.objects;
  // When adding our C++ API into the Sphinx search index we need to specify
  // the type (class, function, etc.) of each API item. Sphinx surfaces this
  // information in the search results so it's important to get it right.
  let ids = {
    function: null,
    functionParam: null,
    class: null,
    templateParam: null,
    member: null,
  };
  // Save the ID for each type of object
  for (const id in objnames) {
    const [domain, type, _] = objnames[id];
    if (domain !== 'cpp') continue;
    ids[type] = id;
  }
  // Get Doxygen data from the tagfile
  // https://www.doxygen.nl/manual/config.html#cfg_generate_tagfile
  const response = await fetch(`${window.pw.root}api/cc/index.tag`);
  const text = await response.text();
  const parser = new DOMParser();
  const xml = parser.parseFromString(text, 'application/xml');
  const root = xml.documentElement;
  // TODO: https://pwbug.dev/446724937 - Figure out what this value does.
  const unknown = 0;
  const anchor = '';
  // Add data found in the Doxygen tagfile to the Sphinx search index
  for (const child of root.children) {
    const kind = child.getAttribute('kind');
    const name = child.querySelector('name').textContent;
    const relativeFilename = child.querySelector('filename').textContent;
    const filename = `api/cc/${relativeFilename}`;
    const docname = filename.replace(/\.html$/, '');
    const title = `${name} class reference`;
    switch (kind) {
      case 'class':
        if (!filenames.includes(filename)) {
          filenames.push(filename);
          docnames.push(docname);
          titles.push(title);
        }
        // `Object.keys(objects)` returns this:
        // `['', 'pigweed', 'pw_build', 'pw_build.recipe', …]`
        // Adding C++ API items to the empty string key works fine. The other
        // keys only contain Python API items.
        objects[''].push([
          filenames.indexOf(filename),
          ids['class'],
          unknown,
          anchor,
          name,
        ]);
        break;
      // TODO: https://pwbug.dev/446724937 - Handle other API items.
      // case 'file':
      //   break;
      // case 'struct':
      //   break;
      // case 'union':
      //   break;
      // case 'namespace':
      //   break;
      default:
        break;
    }
  }
  // When a user opens the search modal, types a query, and then presses the
  // Enter key, Sphinx navigates the user to the dedicated search page
  // (`/search.html`) and runs the search there. In this scenario we need some
  // extra logic to ensure that the search runs against the monkeypatched
  // search index.
  if (!window.location.pathname.endsWith('/search.html')) {
    return;
  }
  const searchInput = document.querySelector('#search-input');
  const query = searchInput.value;
  if (query === '') {
    return;
  }
  // Re-initialize the container that Sphinx renders search results into.
  const oldResults = document.querySelector('#search-results');
  if (oldResults) {
    oldResults.remove();
  }
  let newResults = document.createElement('div');
  newResults.id = 'search-results';
  document.querySelector('div.bd-search-container').appendChild(newResults);
  // Run the search against the complete, monkeypatched search index.
  // eslint-disable-next-line no-undef
  Search.performSearch(query);
};

// Measure how much pigweed.dev visitors use the site's various navigation
// aids: global nav, breadcrumbs, page nav, etc.
window.pw.setUpNavigationAnalytics = () => {
  const selectors = {
    breadcrumbs: '.bd-breadcrumbs a',
    global_nav: '.bd-header a',
    main_content: '.bd-article a',
    page_nav: '.bd-sidebar-secondary a',
    prev_next: '.prev-next-footer a',
    section_nav: '.bd-sidebar-primary a',
  };
  for (const [label, selector] of Object.entries(selectors)) {
    const links = Array.from(document.querySelectorAll(selector));
    links.forEach((link) => {
      link.addEventListener('click', (e) => {
        // eslint-disable-next-line no-undef
        if (typeof window.gtag === 'function') {
          const pw_href = link.href;
          const pw_component = label;
          // eslint-disable-next-line no-undef
          gtag('event', 'pw_navigation', { pw_href, pw_component });
        }
      });
    });
  }
};

window.pw.setUpSearchAnalytics = () => {
  // Report search analytics when the user clicks a SERP link.
  const listen = (node) => {
    // Callers of this function do not verify that the node actually exists.
    if (!node) {
      return;
    }
    node.addEventListener('click', (e) => {
      // Search results always get rendered into a node with this ID.
      const searchResults = node.querySelector('#search-results');
      if (!searchResults) {
        return;
      }
      // The search result link that the user selected.
      const link = e.target.closest('a');
      if (!link) {
        return;
      }
      // eslint-disable-next-line no-undef
      if (typeof window.gtag !== 'function') {
        return;
      }
      const links = Array.from(
        searchResults.querySelectorAll('.search > li > a'),
      );
      const index = links.indexOf(link);
      if (index === -1) {
        return;
      }
      const pw_query = document.querySelector('#search-input').value;
      const pw_href = link.href;
      const pw_rank = index + 1; // Use 1-based indexing for SERP stats.
      // eslint-disable-next-line no-undef
      gtag('event', 'pw_search', { pw_query, pw_href, pw_rank });
    });
  };
  // Parent node of #search-results on /search.html
  listen(document.querySelector('.bd-search-container'));
  // Parent node of #search-results on every page other than /search.html
  listen(document.querySelector('.search-button__search-container'));
};

window.addEventListener('DOMContentLoaded', () => {
  // Manually control when Mermaid diagrams render to prevent scrolling issues.
  // Context: https://pigweed.dev/docs/style_guide.html#site-nav-scrolling
  if (window.mermaid) {
    // https://mermaid.js.org/config/usage.html#using-mermaid-run
    window.mermaid.run();
  }
  // Relative path to the root directory of the site. Sphinx's
  // HTML builder auto-inserts this metadata on every page.
  window.pw.root = document.documentElement.dataset.content_root;
  window.pw.initSearch();
  window.pw.monkeyPatchSphinxSearchIndex();
  window.pw.setUpNavigationAnalytics();
  window.pw.setUpSearchAnalytics();
  window.pw.dev();
});
