<%inherit file="base.mak"/>

<%block name="css">
    <link rel='stylesheet' type='text/css' href='/static/css/skin/ui.dynatree.css'>
    <link rel='stylesheet' type='text/css' href='/static/css/model.css'>
</%block>

<%block name="navbar">
    <ul class="nav" role="navigation">
        <li class="dropdown">
            <a id="file-drop" href="javascript:" role="button" class="dropdown-toggle" data-toggle="dropdown">Model<b class="caret"></b></a>
            <ul class="dropdown-menu" role="menu" aria-labelledby="file-drop">
                <li><a tabindex="-1" href="javascript:">Start New</a></li>
                <li><a tabindex="-1" href="javascript:">Load from file</a></li>
                <li><a tabindex="-1" href="javascript:">Save</a></li>
                <li class="divider"></li>
                <li><a tabindex="-1" href="javascript:">Preferences</a></li>
            </ul>
        </li>
        <li class="dropdown">
            <a id="run-drop" href="javascript:" role="button" class="dropdown-toggle" data-toggle="dropdown">Run<b class="caret"></b></a>
            <ul class="dropdown-menu" role="menu" aria-labelledby="run-drop">
                <li><a tabindex="-1" href="javascript:">Run</a></li>
                <li><a tabindex="-1" href="javascript:">Step</a></li>
                <li><a tabindex="-1" href="javascript:">Run Until...</a></li>
            </ul>
        </li>
        <li class="dropdown">
            <a id="help-drop" href="javascript:" role="button" class="dropdown-toggle" data-toggle="dropdown">Help<b class="caret"></b></a>
            <ul class="dropdown-menu" role="menu" aria-labelledby="help-drop">
                <li><a tabindex="-1" target="window" href="javascript:">About</a></li>
                <li><a tabindex="-1" href="javascript:">Tutorial</a></li>
            </ul>
        </li>
    </ul>
</%block>

<%block name="sidebar">
     <div class="container" id="sidebar-toolbar">
      <div class="btn-toolbar">
        <div class="btn-group">
            <a class="btn" id="add-button" href="javascript:"><i class="icon-plus-sign"></i></a>
            <a class="btn disabled" id="remove-button" href="javascript:"><i class="icon-minus-sign"></i></a>
            <a class="btn disabled" id="settings-button" href="javascript:"><i class="icon-wrench"></i></a>
        </div>
      </div>
    </div>
    <div id="tree">
        <ul id="tree-list">
            <li id="settings" title="Model Settings">
                Model Settings
                <ul>
                    % for setting in model.get_settings():
                            <li id="${setting['name']}">${setting['name']}
                                : ${setting['value']}</li>
                    % endfor

                    <% map = model.get_map() %>
                    % if map:
                        <li>${map['name']}</li>
                    % endif
                </ul>
            </li>
            <li id="movers" title="Movers">
                Movers
                <ul>
                    % for mover in model.get_movers():
                            <li>${mover['name']}</li>
                    % endfor
                </ul>
            </li>
            <li id="spills" title="Spills">
                Spills
                <ul>
                    % for spill in model.get_spills():
                            <li>${spill['name']}</li>
                    % endfor
                </ul>
            </li>
        </ul>
    </div>
</%block>

<%block name="content">
    <div class="container">
       % if warning:
       <div class="alert">
          <button type="button" class="close" data-dismiss="alert">×</button>
          <strong>Warning!</strong> ${warning}
        </div>
      % endif

      <div class="btn-toolbar">
          <div class="btn-group">
              <a class="btn disabled" id="fullscreen-button" href="javascript:"><i class="icon-fullscreen"></i></a>
          </div>
          <div class="btn-group">
              <a class="btn disabled" id="resize-button" href="javascript:"><i class="icon-resize-small"></i></a>
          </div>
          <div class="btn-group">
            <a class="btn disabled" id="zoom-in-button" href="javascript:"><i class="icon-zoom-in"></i></a>
            <a class="btn disabled" id="zoom-out-button" href="javascript:"><i class="icon-zoom-out"></i></a>
            <a class="btn disabled" id="move-button" href="javascript:"><i class="icon-move"></i></a>
        </div>
        <div class="btn-group">
            <a class="btn disabled" id="back-button" href="javascript:"><i class="icon-fast-backward"></i></a>
            <div class="btn disabled" id="slider-container"><span id="time">00:00</span> <div id="slider"></div></div>
            <a class="btn" id="play-button" href="javascript:"><i class="icon-play"></i></a>
            <a class="btn disabled" id="pause-button" href="javascript:"><i class="icon-pause"></i></a>
            <a class="btn disabled" id="forward-button" href="javascript:"><i class="icon-fast-forward"></i></a>
        </div>
      </div>

    </div>
    <div id="map">
        <img class="frame active" data-position="0" src="/static/img/placeholder.gif">
    </div>


    <script id="modalTemplate" type="text/x-jsrender">
        <a href="#modal" role="button" class="btn" data-toggle="modal">Launch demo modal</a>
        <div class="modal hide fade" id="modal" tabindex="-1"
             role="dialog" aria-labelledby="modal-label" aria-hidden="true">
            <div class="modal-header">
                <button type="button" class="close" data-dismiss="mover-modal"
                        aria-hidden="true">×
                </button>
                <h3 id="modal-label">{{ header }}</h3>
            </div>
            <div class="modal-body">
                {{ body }}
            </div>
            <div class="modal-footer">
                <button class="btn" data-dismiss="modal" aria-hidden="true"> Cancel </button>
                <button class="btn btn-primary">Save</button>
            </div>
        </div>
    </script>
</%block>

<%block name="javascript">
    <script src='/static/js/jquery.imagesloaded.min.js' type="text/javascript"></script>
    <script src='/static/js/jquery.cycle.all.latest.js' type="text/javascript"></script>
    <script src='/static/js/jquery.cookie.js' type="text/javascript"></script>
    <script src="/static/js/jquery.dynatree.min.js"></script>
    <script src="/static/js/underscore-min.js"></script>
    <script src="/static/js/map.js"></script>
</%block>