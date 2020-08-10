<h1 style="text-align: center;">Rel&oacute;gio Nixie com Raspbery Pi Zero e detec&ccedil;&atilde;o de face</h1>
<p style="text-align: center;"><em>Eu sou apaixonado por aparelhos valvulados e por rel&oacute;gios.</em><br /><em>Um rel&oacute;gio valvulado ent&atilde;o &eacute; quase de fazer revirar os olhos</em></p>
<p><img src="https://github.com/luizcressoni/nixieclock/blob/master/resources/IMG_20200611_153739.jpg?raw=true" alt="" width="800" height="600" /></p>
<p>&nbsp;</p>
<p><strong>Hist&oacute;ria</strong></p>
<p>Um belo dia, encontrei um lote de Nixies russas com os chips de driver em uma oferta tentadora demais para deixar passar. Comprei o lote e ganhei de brinde uma IN-012 (uma v&aacute;lvula &lsquo;bargraph&rsquo;). Com isso, resolvi projetar um rel&oacute;gio Nixie com tudo que tem direito.</p>
<p>Surgiu ent&atilde;o meu rel&oacute;gio Nixie, com as seguintes caracter&iacute;sticas:</p>
<ul>
<li>6 v&aacute;lvulas Nixie IN-8 para indica&ccedil;&atilde;o de data e hora;</li>
<li>1 v&aacute;lvula&nbsp; IN-012 para indicar temperatura e dia da semana;</li>
<li>6 LEDs RGB, um por baixo de cada v&aacute;lvula para efeitos visuais;</li>
<li>1 Raspberry Pi Zero W como CPU;</li>
<li>1 c&acirc;mera para detec&ccedil;&atilde;o de rostos que serve tamb&eacute;m como &lsquo;spycam&rsquo;;</li>
</ul>
<p>&nbsp;Muitos v&atilde;o dizer que n&atilde;o &eacute; preciso uma Raspberry para fazer um rel&oacute;gio desses, que um ESP32 daria conta ou at&eacute; mesmo um Arduino mais simples.&nbsp;<br />Sim, concordo plenamente. Mas a Raspberry me permite inventar mais, como expandir para comandos por voz (basta adicionar um microfone USB&hellip;) ou o que mais eu quiser, bastando gravar os arquivos no cart&atilde;o - o que pra mim &eacute; mais pr&aacute;tico. Enfim, &eacute; um hardware bem mais poderoso e eu j&aacute; tenho aqui algumas placas.<br />E pelo pre&ccedil;o das v&aacute;lvulas e de tudo mais, o que se economiza n&atilde;o usando a Raspberry &eacute; quase desprez&iacute;vel.<br />De qualquer modo, deixo aqui aberto para quem quiser adaptar o projeto para outra plataforma! Se fizer e quiser compartilhar, me envie o link ou as informa&ccedil;&otilde;es que eu publico aqui.&nbsp;<br />Dividi o projeto em duas partes: Hardware e software. Na parte do hardware, mostro o esquem&aacute;tico&nbsp; e os componentes, deixando o sistema operacional e a aplica&ccedil;&atilde;o obviamente para a parte do software.<br />Para a parte mec&acirc;nica em si escolhi uma bela caixa de charutos antiga que comprei bem baratinho no MercadoLivre.</p>
<p>Seguem algumas imagens do trabalho de prepara&ccedil;&atilde;o da caixa:</p>
<p style="text-align: center;"><img src="https://github.com/luizcressoni/nixieclock/blob/master/resources/IMG_20190615_114315.jpg?raw=true" alt="" width="309" height="412" />&nbsp;&nbsp;<img src="https://github.com/luizcressoni/nixieclock/blob/master/resources/IMG_20190615_114306.jpg?raw=true" alt="" width="309" height="412" /><br /><em>Antes e depois do polimento dos detalhes em lat&atilde;o</em></p>
<p style="text-align: center;"><img src="https://github.com/luizcressoni/nixieclock/blob/master/resources/IMG_20190615_144834.jpg?raw=true" alt="" width="302" height="227" />&nbsp;&nbsp;<img src="https://github.com/luizcressoni/nixieclock/blob/master/resources/IMG_20190615_161908.jpg?raw=true" alt="" width="312" height="234" /><br /><em>Fura&ccedil;&atilde;o para as v&aacute;lvulas</em></p>
<p style="text-align: center;">&nbsp;</p>
<p>No wiki existem arquivos sobre o hardware e software.&nbsp;&nbsp;</p>
<div class="separator" data-original-attrs="{&quot;style&quot;:&quot;&quot;}">&nbsp;</div>
<p style="text-align: center;">&nbsp;</p>
<p style="text-align: center;">&nbsp;</p>
<p>&nbsp;</p>
