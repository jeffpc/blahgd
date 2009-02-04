#include <stdio.h>

#include "sar.h"
#include "html.h"

/************************************************************************/
/*                                POST                                  */
/************************************************************************/
void html_story(struct post *post)
{
	cat(post, "templates/story-top.html", repltab_html);

#if 0
	for_each_category(post)
		cat(stdout, "templates/story-cat-item.html", repltab_html);
#endif

	cat(post, "templates/story-middle.html", repltab_html);

	cat_post(post);

	cat(post, "templates/story-bottom.html", NULL);
}

/************************************************************************/
/*                           POST COMMENTS                              */
/************************************************************************/
void html_comments(struct post *post)
{
	cat(post, "templates/story-comment-head.html", repltab_html);

#if 0
	for_each_comment(post) {
		cat(post, "templates/story-comment-item-head.html", repltab_html);
		cat_post_comment(post, commentid);
		cat(post, "templates/story-comment-item-tail.html", repltab_html);
	}
#endif

	cat(post, "templates/story-comment-tail.html", repltab_html);
}

/************************************************************************/
/*                                 SIDEBAR                              */
/************************************************************************/
void html_sidebar(struct post *post)
{
	cat(post, "templates/sidebar-top.html", repltab_html);
#if 0
	for_each_category()
		cat(post, "templates/sidebar-cat-item.html", repltab_html);
#endif
	cat(post, "templates/sidebar-middle.html", repltab_html);
#if 0
	for_each_month()
		cat(post, "templates/sidebar-archive-item.html", repltab_html);
#endif
	cat(post, "templates/sidebar-bottom.html", repltab_html);
}

/************************************************************************/
/*                             PAGE HEADER                              */
/************************************************************************/
void html_header(struct post *post)
{
	cat(post, "templates/header.html", repltab_html);
}

/************************************************************************/
/*                             PAGE FOOTER                              */
/************************************************************************/
void html_footer(struct post *post)
{
	cat(post, "templates/footer.html", repltab_html);
}
