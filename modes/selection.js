$(document).ready(function(){  
  $("a.decisions_selection_vote").click(function() {
    var link = this;
    var votingDiv = $(link).parents("div.decisions_selection_voting");
    var path = $(link).attr('href');
    $.ajax({
      type: "GET",
      global: false,
      data: 'ajax=true',
      url: path,
      beforeSend:function() {
        $(votingDiv).queue(function() {
          $(votingDiv).fadeOut('slow').dequeue();
        });
      },
      success: function(html) {
        $(link).parents(".decisions_selection_voting").siblings(".decisions_selection_results").html(html);
        //If the page is a WSOD with an HTTP 200 response, that's not successful. This check could be expanded if necessary.
        if (html == '') {
          decisionsErrorText(votingDiv);
        }
        else {
          $(votingDiv).queue(function(){
            $(this).siblings(".decisions_selection_results").fadeIn('slow');
            $(votingDiv).dequeue();
          });
        }
      },
      error: function() {
        decisionsErrorText(votingDiv);
      }
    });
    return false;
  });
  function decisionsErrorText(votingDiv) {
    $(votingDiv).queue(function(){
      $(votingDiv).children(".decisions_selection_error").html('<span class="error">' + Drupal.t('errortext') + '</span>');
      votingDiv.fadeIn('slow');
      $(votingDiv).dequeue();
    });
  }
});